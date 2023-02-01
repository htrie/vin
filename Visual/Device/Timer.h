#pragma once

#include "Visual/Device/Constants.h"

namespace Device
{
	class Timer
	{
	public:
		Timer();

		void Start();
		void Reset();
		void Stop();
		void Step();

		double GetTime();
		double GetElapsedTime();

	protected:
		long long stop_time_us;
		long long last_elapsed_time_us;
		long long base_time_us;

		bool timer_stopped;

		long long GetAdjustedTimeUs();
	};
}