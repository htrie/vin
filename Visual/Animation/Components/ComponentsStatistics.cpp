
#include "ComponentsStatistics.h"


namespace Animation
{
	namespace Components
	{

		static Stats stats;

		const Stats& Stats::GetStats()
		{
			return stats;
		}
		
		void Stats::Add(Type type)
		{
			stats.total_counts[(unsigned)type]++;
		}

		void Stats::Remove(Type type)
		{
			stats.total_counts[(unsigned)type]--;
		}

		void Stats::Tick(Type type)
		{
			stats.ticked_counts[(unsigned)type]++;
		}

		void Stats::Reset()
		{
			stats.ticked_counts.fill(0);
		}

	}
}

