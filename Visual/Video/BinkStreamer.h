#pragma once

#ifdef ENABLE_BINK_VIDEO

#include <memory>

#include <Bink/bink.h>

class BinkStreamer
{
public:
	BinkStreamer();
	~BinkStreamer();

	/*!
	 * This method installs network IO callbacks for Bink. It must be called before streaming.
	 * @return true on success, false if streaming is still in progress.
	 */
	bool Begin();

	/*!
	 * This method restores Bink's default IO callbacks. It must be called when streaming is finished.
	 */
	void End();

	/*!
	 * @return HBINK handle created by Open() call.
	 */
	HBINK GetHandle();

	/*!
	 * This method must be used instead of BinkOpen() when streaming. For arguments description refer to BinkOpen() documentation.
	 * @return true on success (use GetHandle() to retrieve the HBINK handle), otherwise false.
	 */
	bool Open( const char *url, U32 open_flags );
	bool Open( const wchar_t *url, U32 open_flags );

	/*!
	 * This method must be used instead of BinkClose() when streaming.
	 */
	void Close();

private:
	class Impl;
};

#endif