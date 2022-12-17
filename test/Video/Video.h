#pragma once

#ifdef ENABLE_BINK_VIDEO

namespace Device
{
	class IDevice;
}

namespace Video
{
	void VideoInit();
	void VideoQuit();

	void VideoCreateShaders(Device::IDevice* device);
	void VideoDestroyShaders();
	
	struct Video;

	[[nodiscard]] Video* VideoCreate( std::wstring_view filename, std::wstring_view url );
	void VideoDestroy(Video* video);

	void VideoOpen(Video* video);
	void VideoClose(Video* video);

	void VideoCreateTextures(Video* video);
	void VideoDestroyTextures(Video* video);

	void VideoBeforeReset(Video* video);
	void VideoAfterReset(Video* video);

	void VideoRender(Video* video);

	void VideoSetPosition(Video* video, float left, float top, float right, float bottom);
	void VideoSetLoop(Video* video, bool loop);
	void VideoSetVolume(Video* video, float volume);
	void VideoSetAlpha(Video* video, float alpha);
	void VideoSetColourMultiply(Video* video, float r, float g, float b);

	void VideoSetAdditiveBlendMode( Video* video );
	void VideoResetBlendMode( Video* video );

	float VideoGetAspectRatio(Video* video);
	bool VideoIsPlaying(Video* video);

	unsigned int VideoGetNextFrame( Video* video );
	void VideoSetNextFrame( Video* video, unsigned int next_frame );

	float VideoGetPlaceByPercent( Video* video );
	void VideoSetPlaceByPercent( Video* video, float percent );

	bool VideoIsStreaming( Video* video );
}

#endif