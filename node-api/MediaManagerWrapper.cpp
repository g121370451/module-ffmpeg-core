#include "MediaManagerWrapper.h"
#include <MediaProcessor.h>
#include <spdlog/spdlog.h>

Napi::Object MediaManagerWrapper::Init(Napi::Env env, Napi::Object exports)
{
    Napi::Function func = DefineClass(env, "MediaManager",
                                      {
                                          InstanceMethod("addMedia", &MediaManagerWrapper::AddMedia),
                                          InstanceMethod("deleteMedia", &MediaManagerWrapper::DeleteMedia),
                                          InstanceMethod("getNextFrame", &MediaManagerWrapper::GetNextFrame),
                                          InstanceMethod("updateROI", &MediaManagerWrapper::UpdateROI),
                                          InstanceMethod("pause", &MediaManagerWrapper::Pause),
                                          InstanceMethod("resume", &MediaManagerWrapper::Resume),
                                      });

    Napi::FunctionReference *constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("MediaManager", func);
    return exports;
}

MediaManagerWrapper::MediaManagerWrapper(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<MediaManagerWrapper>(info)
{
    _manager = std::make_unique<MediaManager>();
}

// JS: addMedia(deviceId, index, url, x, y, sw, sh, ow, oh)
Napi::Value MediaManagerWrapper::AddMedia(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    std::string devId = info[0].As<Napi::String>();
    int index = info[1].As<Napi::Number>();
    std::string url = info[2].As<Napi::String>();
    try
    {

        ROIConfig config(
            info[3].As<Napi::Number>(), info[4].As<Napi::Number>(),
            info[5].As<Napi::Number>(), info[6].As<Napi::Number>(),
            info[7].As<Napi::Number>(), info[8].As<Napi::Number>());

        auto encoder = std::make_unique<MjpegEncoder>();
        bool res = _manager->AddMedia(devId, index, url, config, std::move(encoder));

        return Napi::Boolean::New(env, res);
    }
    catch (const std::exception &e)
    {
        spdlog::error("AddMedia error: {}", e.what());
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }
    catch (...)
    {
        spdlog::error("AddMedia error");
        Napi::Error::New(env, "AddMedia unknown error").ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }
}

Napi::Value MediaManagerWrapper::DeleteMedia(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    try
    {
        std::string devId = info[0].As<Napi::String>();
        int index = info[1].As<Napi::Number>();

        bool res = _manager->DeleteMedia(devId, index);
        return Napi::Boolean::New(env, res);
    }
    catch (const std::exception &e)
    {
        spdlog::error("Exception in DeleteMedia: {}", e.what());
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }
    catch (...)
    {
        spdlog::error("Exception in DeleteMedia");
        Napi::Error::New(env, "Delete Media unknown error").ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }
}

// JS: updateROI(deviceId, index, x, y, sw, sh)
Napi::Value MediaManagerWrapper::UpdateROI(const Napi::CallbackInfo &info)
{
    _manager->UpdateConfig(
        info[0].As<Napi::String>(),
        info[1].As<Napi::Number>(),
        info[2].As<Napi::Number>(),
        info[3].As<Napi::Number>(),
        info[4].As<Napi::Number>(),
        info[5].As<Napi::Number>());
    return info.Env().Undefined();
}

Napi::Value MediaManagerWrapper::Pause(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    try
    {
        std::string devId = info[0].As<Napi::String>();
        int index = info[1].As<Napi::Number>();

        bool res = _manager->Pause(devId, index);
        return Napi::Boolean::New(env, res);
    }
    catch (const std::exception &e)
    {
        spdlog::error("Exception in Pause: {}", e.what());
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }
    catch (...)
    {
        spdlog::error("Exception in Pause");
        Napi::Error::New(env, "Pause unknown error").ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }
}

Napi::Value MediaManagerWrapper::Resume(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    try
    {
        std::string devId = info[0].As<Napi::String>();
        int index = info[1].As<Napi::Number>();

        bool res = _manager->Resume(devId, index);
        return Napi::Boolean::New(env, res);
    }
    catch (const std::exception &e)
    {
        spdlog::error("Exception in Resume: {}", e.what());
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }
    catch (...)
    {
        spdlog::error("Exception in Resume");
        Napi::Error::New(env, "Resume unknown error").ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }
}

// JS: getNextFrame(deviceId, index) -> { data: Buffer, width, height, success }
Napi::Value MediaManagerWrapper::GetNextFrame(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    std::string devId = info[0].As<Napi::String>();
    int index = info[1].As<Napi::Number>();
    try
    {
        EncoderOutput frame = _manager->GetNextFrame(devId, index);

        Napi::Object obj = Napi::Object::New(env);
        obj.Set("success", Napi::Boolean::New(env, frame.success));

        if (frame.success)
        {
            // 使用 Copy 模式将 C++ vector 数据拷贝到 JS Buffer
            obj.Set("data", Napi::Buffer<uint8_t>::Copy(env, frame.data.data(), frame.data.size()));
            obj.Set("width", Napi::Number::New(env, frame.width));
            obj.Set("height", Napi::Number::New(env, frame.height));
            obj.Set("timesteamp", Napi::Number::New(env, frame.timestamp));
        }

        return obj;
    }
    catch (const std::exception &e)
    {
        spdlog::error("GetNextFrame error: {}", e.what());
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
    catch (...)
    {
        spdlog::error("GetNextFrame error");
        Napi::Error::New(env, "GetNextFrame unknown error").ThrowAsJavaScriptException();
        return env.Null();
    }
}
Napi::Value GetMediaInfoWrap(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    // 1. 参数校验
    if (info.Length() < 1 || !info[0].IsString())
    {
        Napi::TypeError::New(env, "String path expected").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string filePath = info[0].As<Napi::String>().Utf8Value();

    try
    {
        // 2. 调用之前实现的 C++ 静态库方法
        MediaInfo mInfo = MediaProcessor::GetMediaInfo(filePath);

        // 3. 将 C++ struct 映射为 JS Object
        Napi::Object obj = Napi::Object::New(env);
        obj.Set("valid", Napi::Boolean::New(env, mInfo.valid));
        obj.Set("type", Napi::String::New(env, mInfo.type));
        obj.Set("duration", Napi::Number::New(env, mInfo.duration));
        obj.Set("width", Napi::Number::New(env, mInfo.width));
        obj.Set("height", Napi::Number::New(env, mInfo.height));
        obj.Set("sar", Napi::String::New(env, mInfo.sar));
        obj.Set("dar", Napi::String::New(env, mInfo.dar));

        return obj;
    }
    catch (const std::exception &e)
    {
        spdlog::error("getMediaInfo error: {}", e.what());
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
    catch (...)
    {
        spdlog::error("getMediaInfo error");
        Napi::Error::New(env, "GetMediaInfoWrap unknown error").ThrowAsJavaScriptException();
        return env.Null();
    }
}
// 模块导出
Napi::Object InitAll(Napi::Env env, Napi::Object exports)
{
    MediaManagerWrapper::Init(env, exports);
    exports.Set(Napi::String::New(env, "getMediaInfo"), Napi::Function::New(env, GetMediaInfoWrap));
    return exports;
}

NODE_API_MODULE(ffmplayer_node, InitAll)