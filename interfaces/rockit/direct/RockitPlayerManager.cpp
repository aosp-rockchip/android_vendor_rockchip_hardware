/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "RockitPlayerManager"

#include "RockitPlayerManager.h"

#include <utils/Log.h>
#include <dlfcn.h>
#include <fcntl.h>

#include <media/Metadata.h>
#include <media/MediaHTTPService.h>
#include <media/mediaplayer.h>
#include <gui/Surface.h>
#include <system/window.h>
#include <string.h>
#include "RTNativeWindowCallback.h"
#include "RTMsgCallback.h"
#include "RTAudioSinkCallback.h"

namespace android {

using ::android::sp;

typedef struct ROCKIT_PLAYER_CTX {
     sp<RockitPlayer>               mPlayer;
     sp<MediaPlayerBase::AudioSink> mAudioSink;
     RTAudioSinkCallback           *mAudioSinkCB;
     sp<ANativeWindow>              mNativeWindow;
     RTNativeWindowCallback        *mNativeWindowCB;
     RTMsgCallback                 *mMsgCallback;
} RockitPlayerCtx;

RockitPlayerManager::RockitPlayerManager(android::MediaPlayerInterface* mediaPlayer) {
    mCtx = (RockitPlayerCtx *)malloc(sizeof(RockitPlayerCtx));
    memset(mCtx, 0, sizeof(RockitPlayerCtx));
    initPlayer(mediaPlayer);
    ALOGD("RockitPlayerManager(%p) construct", this);
}

RockitPlayerManager::~RockitPlayerManager() {
    ALOGD("~RockitPlayerManager(%p) destruct", this);
    reset();
    deinitPlayer();
    if (mCtx) {
        delete mCtx;
    }
}

void RockitPlayerManager::initPlayer(android::MediaPlayerInterface* mediaPlayer) {
    mCtx->mPlayer = new RockitPlayer();
    rt_status err = mCtx->mPlayer->createPlayer();
    mCtx->mNativeWindowCB = new RTNativeWindowCallback();
    mCtx->mPlayer->setNativeWindowCallback((void *)mCtx->mNativeWindowCB);
    mCtx->mMsgCallback = new RTMsgCallback(mediaPlayer);
    mCtx->mPlayer->setListener(mCtx->mMsgCallback);
    ALOGD("createPlayer err: %d nativeWindowCB: %p", err, mCtx->mNativeWindowCB);
}

void RockitPlayerManager::deinitPlayer() {
    ALOGD("deinitPlayer");
    if (mCtx->mNativeWindowCB != NULL) {
        delete mCtx->mNativeWindowCB;
    }
    if (mCtx->mAudioSinkCB != NULL) {
        delete mCtx->mAudioSinkCB;
    }
    if (mCtx->mMsgCallback != NULL) {
        delete mCtx->mMsgCallback;
    }
    mCtx->mPlayer->destroyPlayer();
}

status_t RockitPlayerManager::initCheck() {
    ALOGV("initCheck");
    return mCtx->mPlayer->initCheck();
}

status_t RockitPlayerManager::setUID(uid_t uid) {
    (void)uid;
    return OK;
}

status_t RockitPlayerManager::setDataSource(
        const sp<IMediaHTTPService> &httpService,
        const char *url,
        const KeyedVector<String8, String8> *headers) {
    (void)httpService;
    (void)headers;
    return (status_t)mCtx->mPlayer->setDataSource(NULL, url, NULL);
}

// Warning: The filedescriptor passed into this method will only be valid until
// the method returns, if you want to keep it, dup it!
status_t RockitPlayerManager::setDataSource(int fd, int64_t offset, int64_t length) {
    ALOGV("setDataSource(%d, %lld, %lld)", fd, (long long)offset, (long long)length);
    char uri[1024] = {0};
    ALOGE("setDataSource: %p", &uri);
    char *params = uri;
    getUriFromFd(fd, &params);
    return (status_t)mCtx->mPlayer->setDataSource(NULL, uri, NULL);
}

status_t RockitPlayerManager::setDataSource(const sp<IStreamSource> &source) {
    (void)source;
    return OK;
}

status_t RockitPlayerManager::setVideoSurfaceTexture(
        const sp<IGraphicBufferProducer> &bufferProducer) {
    ANativeWindow *window = NULL;
    if (bufferProducer.get() != NULL) {
        window = new Surface(bufferProducer, true);
    }
    void *params = (void *)window;
    ALOGV("setVideoSurfaceTexture window: %p bufferProducer: %p", params, bufferProducer.get());
    mCtx->mNativeWindow = window;
    return (status_t)mCtx->mPlayer->setNativeWindow(static_cast<const void *>(window));
}

status_t RockitPlayerManager::prepare() {
    ALOGV("prepare");
    return (status_t)mCtx->mPlayer->prepare();
}

status_t RockitPlayerManager::prepareAsync() {
    ALOGV("prepareAsync");
    return (status_t)mCtx->mPlayer->prepareAsync();
}

status_t RockitPlayerManager::start() {
    ALOGV("start");
    return (status_t)mCtx->mPlayer->start();
}

status_t RockitPlayerManager::stop() {
    ALOGV("stop");
    return (status_t)mCtx->mPlayer->stop();
}

status_t RockitPlayerManager::pause() {
    ALOGV("pause");
    return (status_t)mCtx->mPlayer->pause();
}

bool RockitPlayerManager::isPlaying() {
    ALOGV("isPlaying");
    return mCtx->mPlayer->isPlaying();
}

status_t RockitPlayerManager::seekTo(int msec, MediaPlayerSeekMode mode) {
    ALOGV("seekTo %.2f secs", msec / 1E3);
    return (status_t)mCtx->mPlayer->seekTo(msec, static_cast<uint32_t>(mode));
}

status_t RockitPlayerManager::getCurrentPosition(int *msec) {
    ALOGV("getCurrentPosition");
    return (status_t)mCtx->mPlayer->getCurrentPosition(msec);
}

status_t RockitPlayerManager::getDuration(int *msec) {
    ALOGV("getDuration");
    return (status_t)mCtx->mPlayer->getDuration(msec);
}

status_t RockitPlayerManager::reset() {
    ALOGV("reset");
    return (status_t)mCtx->mPlayer->reset();
}

status_t RockitPlayerManager::setLooping(int loop) {
    ALOGV("setLooping");
    return (status_t)mCtx->mPlayer->setLooping(loop);
}

player_type RockitPlayerManager::playerType() {
    ALOGV("playerType");
    return ROCKIT_PLAYER;
}


status_t RockitPlayerManager::invoke(const Parcel &request, Parcel *reply) {
    ALOGV("setAudioSink invoke");
    return (status_t)mCtx->mPlayer->invoke(request, reply);
}

void RockitPlayerManager::setAudioSink(const sp<MediaPlayerBase::AudioSink> &audioSink) {
    ALOGV("setAudioSink audiosink: %p", audioSink.get());
    mCtx->mAudioSink = audioSink;
    mCtx->mAudioSinkCB = new RTAudioSinkCallback(mCtx->mAudioSink);
    mCtx->mPlayer->setAudioSink((void *)mCtx->mAudioSinkCB);
}

status_t RockitPlayerManager::setParameter(int key, const Parcel &request) {
    ALOGV("setParameter(key=%d)", key);
    return (status_t)mCtx->mPlayer->setParameter(key, request);
}

status_t RockitPlayerManager::getParameter(int key, Parcel *reply) {
    ALOGV("getParameter");
    (void)key;
    (void)reply;
    return OK;//reinterpret_cast<status_t>(mPlayer->getParameter(key, reply));
}

status_t RockitPlayerManager::getMetadata(
        const media::Metadata::Filter& ids, Parcel *records) {
    ALOGV("getMetadata");
    (void)ids;
    (void)records;
    return OK;//reinterpret_cast<status_t>(mPlayer->getMetadata(ids, records));
}

status_t RockitPlayerManager::getPlaybackSettings(AudioPlaybackRate* rate) {
    (void)rate;
    return OK;//reinterpret_cast<status_t>(mPlayer->getPlaybackSettings(rate));
}

status_t RockitPlayerManager::setPlaybackSettings(const AudioPlaybackRate& rate) {
    (void)rate;
    return OK;//reinterpret_cast<status_t>(mPlayer->setPlaybackSettings(rate));
}

status_t RockitPlayerManager::dump(int fd, const Vector<String16> &args) const {
    (void)args;
    (void)fd;
    return OK;//reinterpret_cast<status_t>(mPlayer->dump(args));
}

sp<MediaPlayerBase::AudioSink> RockitPlayerManager::getAudioSink() {
    return mCtx->mAudioSink;
}


status_t RockitPlayerManager::getUriFromFd(int fd, char **uri) {
    size_t      uriSize = 0;
    char        uriTmp[1024] = {0};
    const char *ptr = NULL;
    String8     path;

    path.appendFormat("/proc/%d/fd/%d", getpid(), fd);
    if ((uriSize = readlink(path.string(), uriTmp, sizeof(uriTmp) - 1)) < 0) {
        return BAD_VALUE;
    } else {
        uriTmp[uriSize] = '\0';
    }

    path = uriTmp;
    ptr  = path.string();
    ALOGD("getUriFromFd ptr: %p, uriSize: %zu, uri: %s *uri: %p uri: %p",
           ptr, uriSize, ptr, *uri, uri);
    memcpy(*uri, ptr, uriSize);

    return OK;
}

}  // namespace android


