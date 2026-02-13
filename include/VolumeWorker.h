#pragma once
#include "Utils.h"
#include "AudioManager.h"
#include <atomic>
#include <array>
#include <atomic>  
#include <string>  
#include <mutex>
#include <condition_variable>

namespace VolumeWorker {

    extern int &DISCORD_SINK;
    extern int &SPOTIFY_SINK;
    extern int &YOUTUBE_SINK;
    extern int &VLC_SINK;

    enum class VolumeType { Sink, Speaker };

    struct VolumeState {
      std::atomic<int> target;
      std::atomic<int> current;
    };

    struct VolumeChannel {
      VolumeType type;
      VolumeState volume;

      int *sinkID = nullptr;
      std::string speaker;
    };

    void change_sink_volume(int readPercent, std::atomic<int> &knobPercent, int &sinkId, std::array<int, 4> &sinks);
    void change_speaker_volume(int readPercent, std::atomic<int> &knobPercent, std::string device);
    void apply_volume(VolumeChannel& channel);
    void async_worker();
    void notify_worker();
    std::array<int, 4> return_sinks();

    extern VolumeChannel discord;
    extern VolumeChannel spotify;
    extern VolumeChannel vlc;
    extern VolumeChannel youtube;
    extern VolumeChannel speaker;

    extern std::array<int, 4> sink;
    extern bool sink_lock;

    
};

