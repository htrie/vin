
namespace Device
{
	Timer::Timer()
		: timer_stopped(true)
		, stop_time_us(0)
		, last_elapsed_time_us(0)
		, base_time_us(0)
	{
	}

	void Timer::Start()
	{
		const auto time_us = HighResTimer::Get().GetTimeUs();
		if (timer_stopped)
			base_time_us += time_us - stop_time_us;
		stop_time_us = 0;
		last_elapsed_time_us = time_us;
		timer_stopped = false;
	}

	void Timer::Reset()
	{
		const auto adjusted_time_us = GetAdjustedTimeUs();
		base_time_us = adjusted_time_us;
		last_elapsed_time_us = adjusted_time_us;
		stop_time_us = 0;
		timer_stopped = false;
	}

	void Timer::Stop()
	{
		if (timer_stopped)
			return;

		const auto time_us = HighResTimer::Get().GetTimeUs();
		stop_time_us = time_us;
		last_elapsed_time_us = time_us;
		timer_stopped = true;
	}

	void Timer::Step()
	{
		stop_time_us += 10000; // 0.1s
	}

	double Timer::GetTime()
	{
		const auto adjusted_time_us = GetAdjustedTimeUs();
		return (double)(adjusted_time_us - base_time_us) / 1000000.0;
	}

	double Timer::GetElapsedTime()
	{
		const auto adjusted_time_us = GetAdjustedTimeUs();
		double elapsed_time = (double)(adjusted_time_us - last_elapsed_time_us) / 1000000.0;
		last_elapsed_time_us = adjusted_time_us;
		if (elapsed_time < 0.0)
			elapsed_time = 0.0;
		return (float)elapsed_time;
	}

	long long Timer::GetAdjustedTimeUs()
	{
		if (stop_time_us != 0)
			return stop_time_us;
		return HighResTimer::Get().GetTimeUs();
	}

}