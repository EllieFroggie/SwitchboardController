#include "VolumeWorker.h"

namespace VolumeWorker {
  std::array<int, 4> sink = {-1, -1, -1, -1}; // 0 Vesktop, 1 Spotify, 2 Youtube, 3 VLC
  bool sink_lock = false;

  int &DISCORD_SINK = sink[0];
  int &SPOTIFY_SINK = sink[1];
  int &YOUTUBE_SINK = sink[2];
  int &VLC_SINK = sink[3];

  std::mutex workerMutex;
  std::condition_variable workerCv;
  std::atomic<bool> workPending = false;


  VolumeChannel discord{VolumeType::Sink, {120, 120}, &DISCORD_SINK};

  VolumeChannel spotify{VolumeType::Sink, {40, 40}, &SPOTIFY_SINK};

  VolumeChannel vlc{VolumeType::Sink, {45, 45}, &VLC_SINK};

  VolumeChannel youtube{VolumeType::Sink, {40, 40}, &YOUTUBE_SINK};

  VolumeChannel speaker{VolumeType::Speaker, {100, 100}, nullptr, "alsa_output.pci-0000_00_1f.3.analog-stereo"};

  void change_sink_volume(int readPercent, std::atomic<int> &knobPercent, int &sinkId, std::array<int, 4> &sinks) {
  if (sinkId != -1) {
    try {
      Utils::exec_cmd(std::string("pactl set-sink-input-volume " + std::to_string(sinkId) + " " + std::to_string(readPercent) + "%  2>/dev/null").c_str());          
      knobPercent.store(readPercent);
      return;
    } catch (const std::runtime_error &e) {
      #ifdef DEBUG
        std::cout << "change_sink_volume(): Error in change_sink_volume(): " << e.what() << std::endl;
      #endif
      sinkId = -1;
    }
  }
}

void change_speaker_volume(int readPercent, std::atomic<int> &knobPercent, std::string device) {
    Utils::exec_cmd(std::string("pactl set-sink-volume " + device + " " + std::to_string(readPercent) + "%").c_str());
    knobPercent.store(readPercent);
}

void apply_volume(VolumeChannel& channel) {
  int target = channel.volume.target.load();
  int current = channel.volume.current.load();

  if (target == current) return;

  if (channel.type == VolumeType::Sink && channel.sinkID && *channel.sinkID != -1) {
    change_sink_volume(target, channel.volume.current, *channel.sinkID, sink);
  } else if (channel.type == VolumeType::Speaker) {
    change_speaker_volume(target, channel.volume.current, channel.speaker);
  }
}

void async_worker() {

  std::unique_lock<std::mutex> lock(workerMutex);
  auto nextRefresh = std::chrono::steady_clock::now() +  std::chrono::seconds(5);

    while (true) {

      workerCv.wait_until(lock, nextRefresh, [] { return workPending.load(); });

      bool doWork = workPending.exchange(false);
      bool doRefresh = std::chrono::steady_clock::now() >= nextRefresh;

      lock.unlock();

      if (doWork) {
        apply_volume(discord);
        apply_volume(spotify);
        apply_volume(vlc);
        apply_volume(youtube);
        apply_volume(speaker);
      }

      if (doRefresh && !sink_lock) {
        AudioManager::get_all_sinks(sink, sink_lock);
        nextRefresh = std::chrono::steady_clock::now() + std::chrono::seconds(2);
      }

      lock.lock();
    }
  }

  void notify_worker() {
    {
      std::lock_guard<std::mutex> lock(workerMutex);
      workPending = true;
    }
    workerCv.notify_one();
  }

  


}