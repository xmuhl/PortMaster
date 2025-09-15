// Simple headless test runner focusing on mode switching and responsiveness.
// Builds against in-repo Protocol/ and Transport/ components without UI.

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <random>
#include <iomanip>
#include <filesystem>

#include "Transport/LoopbackTransport.h"
#include "Protocol/ReliableChannel.h"

using namespace std::chrono;

struct CaseResult {
    std::string name;
    bool passed = false;
    std::string message;
    double ms = 0.0;
};

static std::vector<uint8_t> randomBytes(size_t n, uint32_t seed = 1234567) {
    std::vector<uint8_t> v(n);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 255);
    for (size_t i = 0; i < n; ++i) v[i] = static_cast<uint8_t>(dist(rng));
    return v;
}

static void logLine(std::ofstream& ofs, const std::string& s) {
    auto now = system_clock::to_time_t(system_clock::now());
    ofs << "[" << std::put_time(std::localtime(&now), "%F %T") << "] " << s << "\n";
}

static CaseResult TestRawLoopbackBasic(const std::string& logPath) {
    CaseResult r; r.name = "raw_loopback_basic";
    std::ofstream ofs(logPath, std::ios::app);
    logLine(ofs, "Start TestRawLoopbackBasic");

    auto transport = std::make_shared<LoopbackTransport>();
    TransportConfig cfg; // defaults are fine
    auto t0 = high_resolution_clock::now();
    if (!transport->Open(cfg)) {
        r.message = "Open() failed";
        return r;
    }

    std::mutex mtx; std::condition_variable cv; bool done = false; std::vector<uint8_t> got;
    transport->SetDataReceivedCallback([&](const std::vector<uint8_t>& data){
        std::lock_guard<std::mutex> lk(mtx);
        got.insert(got.end(), data.begin(), data.end());
        done = true;
        cv.notify_all();
    });

    auto expect = randomBytes(4096);
    auto w0 = high_resolution_clock::now();
    size_t written = transport->Write(expect);
    auto w1 = high_resolution_clock::now();
    double write_ms = (double)duration_cast<milliseconds>(w1 - w0).count();
    ofs << "Write returned=" << written << ", ms=" << write_ms << "\n";

    // Wait for callback
    {
        std::unique_lock<std::mutex> lk(mtx);
        if (!cv.wait_for(lk, std::chrono::seconds(2), [&]{return done;})) {
            r.message = "Timeout waiting for loopback";
            transport->Close();
            return r;
        }
    }

    transport->Close();
    auto t1 = high_resolution_clock::now();
    r.ms = (double)duration_cast<milliseconds>(t1 - t0).count();
    r.passed = (got == expect) && (written == expect.size()) && (write_ms < 100.0);
    if (!r.passed) {
        r.message = (got != expect ? "mismatch" : (write_ms >= 100.0 ? "write too slow" : "unknown"));
    }
    logLine(ofs, std::string("End TestRawLoopbackBasic: ") + (r.passed?"PASS":"FAIL"));
    return r;
}

static CaseResult TestReliableLoopbackBasic(const std::string& logPath) {
    CaseResult r; r.name = "reliable_loopback_basic";
    std::ofstream ofs(logPath, std::ios::app);
    logLine(ofs, "Start TestReliableLoopbackBasic");

    auto transport = std::make_shared<LoopbackTransport>();
    TransportConfig cfg; 
    if (!transport->Open(cfg)) { r.message = "Open() failed"; return r; }

    ReliableChannel ch(transport);
    ch.EnableReceiving(true);
    ch.SetWindowSize(8);
    ch.SetAckTimeout(200);
    ch.SetMaxRetries(5);
    ch.SetReceiveDirectory(".");

    std::mutex mtx; std::condition_variable cv; bool completed = false; bool success = false;
    ch.SetCompletionCallback([&](bool ok, const std::string& msg){
        std::lock_guard<std::mutex> lk(mtx);
        success = ok; completed = true; cv.notify_all();
        ofs << "Completion: ok=" << ok << ", msg=" << msg << "\n";
    });

    auto data = randomBytes(64 * 1024 + 123); // cross multiple frames
    if (!ch.Start()) { r.message = "Start() failed"; transport->Close(); return r; }

    auto w0 = high_resolution_clock::now();
    bool sent = ch.SendData(data);
    auto w1 = high_resolution_clock::now();
    double api_ms = (double)duration_cast<milliseconds>(w1 - w0).count();
    ofs << "SendData returned=" << sent << ", ms=" << api_ms << "\n";

    // Wait for completion
    {
        std::unique_lock<std::mutex> lk(mtx);
        if (!cv.wait_for(lk, std::chrono::seconds(5), [&]{return completed;})) {
            r.message = "Timeout waiting completion";
            ch.Stop();
            return r;
        }
    }

    ch.Stop();
    r.passed = sent && success && api_ms < 100.0; // API should be non-blocking-ish
    if (!r.passed) {
        r.message = (!sent?"SendData failed":(!success?"completion not success":"API blocked"));
    }
    logLine(ofs, std::string("End TestReliableLoopbackBasic: ") + (r.passed?"PASS":"FAIL"));
    return r;
}

static CaseResult TestModeSwitchStress(const std::string& logPath) {
    CaseResult r; r.name = "mode_switch_stress";
    std::ofstream ofs(logPath, std::ios::app);
    logLine(ofs, "Start TestModeSwitchStress");

    auto transport = std::make_shared<LoopbackTransport>();
    TransportConfig cfg; 
    if (!transport->Open(cfg)) { r.message = "Open() failed"; return r; }

    ReliableChannel ch(transport);
    ch.EnableReceiving(true);
    ch.SetWindowSize(8);
    ch.SetAckTimeout(200);
    ch.SetMaxRetries(3);

    int cycles = 6; bool allOk = true;
    for (int i = 0; i < cycles; ++i) {
        ofs << "Cycle " << i+1 << "/" << cycles << " (RELIABLE)\n";
        if (!transport->IsOpen()) { if (!transport->Open(cfg)) { allOk = false; break; } }
        if (!ch.Start()) { allOk = false; break; }

        std::mutex mtx; std::condition_variable cv; bool done=false; bool ok=false;
        ch.SetCompletionCallback([&](bool s, const std::string& msg){
            std::lock_guard<std::mutex> lk(mtx); (void)msg; done=true; ok=s; cv.notify_all();
        });
        auto data = randomBytes(12 * 1024 + i*31, 424242 + i);
        bool sent = ch.SendData(data);
        if (!sent) { allOk = false; break; }
        {
            std::unique_lock<std::mutex> lk(mtx);
            if (!cv.wait_for(lk, std::chrono::seconds(4), [&]{return done;})) { allOk = false; break; }
        }
        ch.Stop(); // Stop reliable closes transport

        // Re-open transport for RAW and test echo
        ofs << "Cycle " << i+1 << "/" << cycles << " (RAW)\n";
        if (!transport->Open(cfg)) { allOk = false; break; }
        std::mutex m2; std::condition_variable cv2; bool got=false; std::vector<uint8_t> echo;
        transport->SetDataReceivedCallback([&](const std::vector<uint8_t>& d){
            std::lock_guard<std::mutex> lk(m2); echo.insert(echo.end(), d.begin(), d.end()); got=true; cv2.notify_all();
        });
        auto raw = randomBytes(1024 + i*13, 777 + i);
        size_t w = transport->Write(raw);
        {
            std::unique_lock<std::mutex> lk(m2);
            if (!cv2.wait_for(lk, std::chrono::seconds(2), [&]{return got;})) { allOk = false; break; }
        }
        if (w != raw.size() || echo != raw) { allOk = false; break; }
        transport->Close();
    }

    r.passed = allOk;
    if (!r.passed) r.message = "failure in one of cycles";
    logLine(ofs, std::string("End TestModeSwitchStress: ") + (r.passed?"PASS":"FAIL"));
    return r;
}

int main(int argc, char** argv) {
    std::string outDir = "artifacts/test-results";
    std::string logPath = outDir + std::string("/ModeSwitchRunner.log");
    // Ensure directory exists (best-effort)
    std::error_code ec; std::filesystem::create_directories(outDir, ec);

    auto r1 = TestRawLoopbackBasic(logPath);
    auto r2 = TestReliableLoopbackBasic(logPath);
    auto r3 = TestModeSwitchStress(logPath);

    std::ofstream summary(outDir + "/summary.txt", std::ios::trunc);
    summary << "Test Summary\n";
    for (const auto& r : {r1, r2, r3}) {
        summary << r.name << ": " << (r.passed?"PASS":"FAIL");
        if (!r.message.empty()) summary << " - " << r.message;
        summary << "\n";
    }
    summary.close();

    bool ok = r1.passed && r2.passed && r3.passed;
    std::cout << (ok?"ALL PASS":"FAIL") << std::endl;
    return ok ? 0 : 1;
}

