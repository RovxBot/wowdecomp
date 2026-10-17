#include "Engine/Source/Services/Portable/AsyncFile.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

using namespace Services::Native;
using namespace std::chrono_literals;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Gate {
    std::mutex mutex; std::condition_variable changed; bool entered = false, released = false;
    ReadResult Read(const Cancellation&) {
        std::unique_lock<std::mutex> lock(mutex); entered = true; changed.notify_all();
        changed.wait(lock,[&]{return released;}); return {};
    }
    void Wait() {
        std::unique_lock<std::mutex> lock(mutex);
        Check(changed.wait_for(lock,3s,[&]{return entered;}),"Worker did not start");
    }
    void Release() { std::lock_guard<std::mutex> lock(mutex); released=true; changed.notify_all(); }
    ~Gate() { Release(); }
};
int main() {
    try {
        auto owner = std::this_thread::get_id();
        for (size_t iteration=0;iteration<3;++iteration) {
            Gate gate;
            std::vector<int> reads, callbacks;
            FileQueue queue;
            auto callback = [&](int id, ReadStatus expected) {
                return [&,id,expected](const ReadResult& result) {
                    Check(std::this_thread::get_id()==owner,"Callback left owner thread");
                    Check(result.status==expected,"Wrong async terminal status"); callbacks.push_back(id);
                };
            };
            auto active=queue.Submit([&](const Cancellation& c){return gate.Read(c);},callback(1,ReadStatus::Cancelled));
            gate.Wait();
            Check(!queue.Wait(active,5ms),"Blocked read ignored timeout");
            auto submit=[&](int id,uint8_t priority,bool urgent=false) {
                return queue.Submit([&,id](const Cancellation&){reads.push_back(id);return ReadResult{};},callback(id,ReadStatus::Success),priority,urgent);
            };
            auto second=submit(2,20), third=submit(3,20), urgent=submit(4,20,true), promoted=submit(5,80);
            auto cancelled=queue.Submit([](const Cancellation&)->ReadResult{throw std::runtime_error("Cancelled queued work executed");},callback(6,ReadStatus::Cancelled),0);
            Check(queue.Cancel(cancelled)&&queue.Cancel(active),"Queued/running cancellation failed");
            Check(queue.Promote(promoted,10)&&!queue.Promote(third,50),"Priority promotion failed");
            gate.Release();
            Check(queue.WaitAll(5s),"Async wait-all timed out");
            Check(reads==std::vector<int>({5,4,2,3}),"Priority/urgent/FIFO ordering changed");
            Check(callbacks.size()==6 && FileQueue::Complete(active) && !queue.Cancel(active),"Terminal callback count/cancel state failed");
            auto exception=queue.Submit([](const Cancellation&)->ReadResult{throw std::runtime_error("fixture read failure");},[&](const ReadResult& result){Check(result.status==ReadStatus::Failed && result.error=="fixture read failure","Read failure hidden");});
            Check(queue.Wait(exception,3s),"Failed read was never delivered");
            size_t nested=0;
            queue.Submit([](const Cancellation&){return ReadResult{};},[&](const ReadResult&){
                queue.Submit([](const Cancellation&){return ReadResult{};},[&](const ReadResult&){++nested;});
            });
            Check(queue.WaitAll(3s)&&nested==1,"Callback submission/lifetime failed");
            bool affinity=false;
            std::thread wrongThread([&]{try{queue.Poll();}catch(const std::logic_error&){affinity=true;}}); wrongThread.join();
            Check(affinity,"Foreign callback pump accepted");
            auto path=std::filesystem::temp_directory_path()/ ("wowdecomp-async-fixture-"+std::to_string(iteration));
            {std::ofstream file(path,std::ios::binary);file<<"abcdef";}
            size_t partial=0;
            queue.ReadHost(path.u8string(),4,100,[&](const ReadResult& result){
                Check(result.status==ReadStatus::Success && result.file.bytes==Blizzard::Mopaq::Native::Bytes({'e','f'}),"Partial EOF read failed");++partial;
            });
            queue.ReadHost(path.u8string()+"-absent",0,4,[&](const ReadResult& result){Check(result.status==ReadStatus::Failed && result.systemError,"Host failure hidden");++partial;});
            Check(queue.WaitAll(3s)&&partial==2,"Host read callbacks missing");
            std::filesystem::remove(path);
            std::vector<std::thread> producers;
            size_t delivered=0;
            for(size_t p=0;p<4;++p) producers.emplace_back([&]{for(size_t j=0;j<100;++j)
                queue.Submit([](const Cancellation&){return ReadResult{};},[&](const ReadResult&){++delivered;});});
            for(auto& producer:producers)producer.join();
            Check(queue.WaitAll(5s)&&delivered==400,"Concurrent producer requests lost");
            queue.Shutdown(); Check(!queue.Pending(),"Queue leaked pending requests");
            bool rejected=false;try{queue.Submit([](const Cancellation&){return ReadResult{};},[](const ReadResult&){});}catch(const std::logic_error&){rejected=true;}
            Check(rejected,"Stopped queue accepted new work");
        }
        {
            FileQueue queue;
            size_t callbacks=0;
            for(size_t i=0;i<100;++i)queue.Submit([](const Cancellation&){return ReadResult{};},[&](const ReadResult&){++callbacks;});
            queue.Shutdown();Check(callbacks==100&&!queue.Pending(),"Shutdown lost callbacks");
        }
        {
            FileQueue queue;
            auto ticket=queue.Submit([](const Cancellation&){return ReadResult{};},[](const ReadResult&){throw std::runtime_error("fixture callback exception");});
            bool reported=false;
            try{queue.WaitAll(3s);}catch(const std::runtime_error&){reported=true;}
            Check(reported && FileQueue::Complete(ticket),"Callback exception lost terminal state");
            queue.Shutdown();
        }
        Check(!OsLiveThreadCount(),"File worker survived queue destruction");
        std::cout<<"Async priority/FIFO, queued/current cancellation, owner callbacks, partial I/O, timeouts, failures, producers and repeated shutdown passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
