// 婊戝姩绐楀彛鍗忚娴嬭瘯绋嬪簭
// 鐢ㄤ簬楠岃瘉婊戝姩绐楀彛瀹炵幇鐨勫熀鏈€昏緫

#include <iostream>
#include <vector>
#include <map>
#include <chrono>
#include <cassert>

// 妯℃嫙甯х粨鏋?
struct TestFrame {
    uint16_t sequence;
    std::vector<uint8_t> data;
    
    TestFrame(uint16_t seq, const std::vector<uint8_t>& d) : sequence(seq), data(d) {}
};

// 妯℃嫙鍙戦€佺獥鍙ｆ潯鐩?
struct TestSendWindowEntry {
    uint16_t sequence;
    TestFrame frame;
    std::chrono::steady_clock::time_point sendTime;
    int retryCount;
    
    TestSendWindowEntry(uint16_t seq, const TestFrame& f) 
        : sequence(seq), frame(f), sendTime(std::chrono::steady_clock::now()), retryCount(0) {}
};

// 婊戝姩绐楀彛鍗忚娴嬭瘯绫?
class SlidingWindowTest {
private:
    uint8_t windowSize = 8;
    uint16_t sendBase = 1;
    uint16_t nextSequence = 1;
    std::map<uint16_t, TestSendWindowEntry> sendWindow;
    
    // 妯℃嫙鎺ユ敹绐楀彛
    std::map<uint16_t, std::vector<uint8_t>> receiveWindow;
    uint16_t receiveBase = 2;  // START=1, DATA浠?寮€濮?
    std::vector<uint8_t> deliveredData;

public:
    // 娴嬭瘯1锛氱獥鍙ｅぇ灏忛檺鍒?
    void testWindowSizeLimit() {
        std::cout << "娴嬭瘯1锛氱獥鍙ｅぇ灏忛檺鍒?.." << std::endl;
        
        sendWindow.clear();
        sendBase = 1;
        nextSequence = 1;
        
        // 灏濊瘯鍙戦€佽秴杩囩獥鍙ｅぇ灏忕殑甯?
        int sentFrames = 0;
        for (int i = 0; i < windowSize + 3; i++) {
            if (canSendNewFrame()) {
                std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
                TestFrame frame(nextSequence, data);
                sendFrameInWindow(frame);
                nextSequence++;
                sentFrames++;
            } else {
                break;
            }
        }
        
        assert(sentFrames == windowSize);
        assert(sendWindow.size() == windowSize);
        std::cout << "鉁?绐楀彛澶у皬闄愬埗姝ｇ‘锛屽彂閫佷簡 " << sentFrames << " 涓抚" << std::endl;
    }
    
    // 娴嬭瘯2锛氱疮绉疉CK澶勭悊
    void testCumulativeAck() {
        std::cout << "娴嬭瘯2锛氱疮绉疉CK澶勭悊..." << std::endl;
        
        // 鍏堝～婊＄獥鍙?
        testWindowSizeLimit();
        
        size_t initialWindowSize = sendWindow.size();
        
        // 鍙戦€佺疮绉疉CK for sequence 5 (搴旇纭搴忓彿1-5)
        handleWindowAck(5);
        
        assert(sendBase == 6);  // sendBase搴旇鏇存柊涓?
        assert(sendWindow.size() == initialWindowSize - 5);  // 搴旇閲婃斁5涓獥鍙ｄ綅缃?
        
        std::cout << "鉁?绱НACK澶勭悊姝ｇ‘锛岀獥鍙ｉ噴鏀句簡5涓綅缃? << std::endl;
    }
    
    // 娴嬭瘯3锛氫贡搴忔帴鏀跺拰鎸夊簭鎻愪氦
    void testOutOfOrderReceive() {
        std::cout << "娴嬭瘯3锛氫贡搴忔帴鏀跺拰鎸夊簭鎻愪氦..." << std::endl;
        
        receiveWindow.clear();
        receiveBase = 2;
        deliveredData.clear();
        
        // 妯℃嫙鎺ユ敹涔卞簭甯э細鏀跺埌seq 3, 4, 2锛堟寜杩欎釜椤哄簭锛?
        std::vector<uint8_t> data2 = {2};
        std::vector<uint8_t> data3 = {3};
        std::vector<uint8_t> data4 = {4};
        
        // 鍏堟敹鍒皊eq 3锛堜贡搴忥級
        handleDataFrame(3, data3);
        assert(deliveredData.empty());  // 涓嶅簲璇ユ彁浜わ紝鍥犱负seq 2杩樻湭鍒?
        
        // 鍐嶆敹鍒皊eq 4锛堢户缁贡搴忥級
        handleDataFrame(4, data4);
        assert(deliveredData.empty());  // 浠嶇劧涓嶅簲璇ユ彁浜?
        
        // 鏈€鍚庢敹鍒皊eq 2锛堟寜搴忥級
        handleDataFrame(2, data2);
        
        // 鐜板湪搴旇鎸夊簭鎻愪氦2, 3, 4
        assert(deliveredData.size() == 3);
        assert(deliveredData[0] == 2);
        assert(deliveredData[1] == 3);
        assert(deliveredData[2] == 4);
        assert(receiveBase == 5);  // receiveBase搴旇鎺ㄨ繘鍒?
        
        std::cout << "鉁?涔卞簭鎺ユ敹鍜屾寜搴忔彁浜ゆ纭? << std::endl;
    }
    
    // 娴嬭瘯4锛氬揩閫熼噸浼?
    void testFastRetransmit() {
        std::cout << "娴嬭瘯4锛氬揩閫熼噸浼?.." << std::endl;
        
        sendWindow.clear();
        sendBase = 1;
        nextSequence = 1;
        
        // 鍙戦€佸嚑涓抚
        for (int i = 0; i < 3; i++) {
            std::vector<uint8_t> data = {static_cast<uint8_t>(i + 1)};
            TestFrame frame(nextSequence, data);
            sendFrameInWindow(frame);
            nextSequence++;
        }
        
        // 妯℃嫙鏀跺埌seq 2鐨凬AK锛堝揩閫熼噸浼犺姹傦級
        auto it = sendWindow.find(2);
        assert(it != sendWindow.end());
        
        int originalRetryCount = it->second.retryCount;
        handleWindowNak(2);
        
        // 妫€鏌ラ噸浼犺鏁版槸鍚﹀鍔?
        it = sendWindow.find(2);
        assert(it != sendWindow.end());
        assert(it->second.retryCount == originalRetryCount + 1);
        
        std::cout << "鉁?蹇€熼噸浼犺Е鍙戞纭? << std::endl;
    }
    
    void runAllTests() {
        std::cout << "=== 婊戝姩绐楀彛鍗忚娴嬭瘯寮€濮?===" << std::endl;
        
        testWindowSizeLimit();
        testCumulativeAck();
        testOutOfOrderReceive();
        testFastRetransmit();
        
        std::cout << "=== 鎵€鏈夋祴璇曢€氳繃锛佹粦鍔ㄧ獥鍙ｅ崗璁疄鐜版纭?===" << std::endl;
    }

private:
    bool canSendNewFrame() const {
        return sendWindow.size() < windowSize;
    }
    
    bool sendFrameInWindow(const TestFrame& frame) {
        if (!canSendNewFrame()) {
            return false;
        }
        
        sendWindow.emplace(frame.sequence, TestSendWindowEntry(frame.sequence, frame));
        return true;
    }
    
    void handleWindowAck(uint16_t sequence) {
        // 绱НACK锛氱‘璁ゆ墍鏈?<= sequence 鐨勫抚
        auto it = sendWindow.begin();
        while (it != sendWindow.end()) {
            if (it->first <= sequence) {
                it = sendWindow.erase(it);
            } else {
                ++it;
            }
        }
        
        // 鏇存柊鍙戦€佸熀搴忓彿
        if (sequence >= sendBase) {
            sendBase = sequence + 1;
        }
    }
    
    void handleWindowNak(uint16_t sequence) {
        auto it = sendWindow.find(sequence);
        if (it != sendWindow.end()) {
            it->second.retryCount++;
        }
    }
    
    void handleDataFrame(uint16_t sequence, const std::vector<uint8_t>& data) {
        // 缂撳瓨涔卞簭甯?
        receiveWindow[sequence] = data;
        
        // 灏濊瘯鎸夊簭鎻愪氦
        deliverOrderedFrames();
    }
    
    void deliverOrderedFrames() {
        while (true) {
            auto it = receiveWindow.find(receiveBase);
            if (it == receiveWindow.end()) {
                break;  // 鏈熸湜鐨勫簭鍙峰抚涓嶅瓨鍦?
            }
            
            // 鎸夊簭鎻愪氦鏁版嵁
            deliveredData.insert(deliveredData.end(), it->second.begin(), it->second.end());
            
            // 绉婚櫎宸插鐞嗙殑甯?
            receiveWindow.erase(it);
            receiveBase++;
        }
    }
};

int main() {
    SlidingWindowTest test;
    test.runAllTests();
    return 0;
}