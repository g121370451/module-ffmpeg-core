#pragma once
#include <napi.h>
#include "MediaManager.h"

class MediaManagerWrapper : public Napi::ObjectWrap<MediaManagerWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    MediaManagerWrapper(const Napi::CallbackInfo& info);

private:
    // JS 映射方法
    Napi::Value AddMedia(const Napi::CallbackInfo& info);
    Napi::Value DeleteMedia(const Napi::CallbackInfo &info);
    Napi::Value GetNextFrame(const Napi::CallbackInfo &info);
    Napi::Value UpdateROI(const Napi::CallbackInfo& info);
    Napi::Value Pause(const Napi::CallbackInfo& info);
    Napi::Value Resume(const Napi::CallbackInfo& info);

    std::unique_ptr<MediaManager> _manager;
};