#pragma once
#include <vector>
#include <assert.h>
template<typename T>
struct CachedArray
{
public:
	CachedArray()
	{
		this->contentsChanged = true;
	}
	T &GetById(size_t elementId)
	{
		return elements[elementId];
	}
	T &GetByIndex(size_t elementIndex)
	{
		return elements[aliveIds[elementIndex]];
	}
	T &operator[](size_t elementIndex)
	{
		return elements[aliveIds[elementIndex]];
	}
	size_t GetId(size_t elementIndex)
	{
		return aliveIds[elementIndex];
	}
	size_t GetIndex(size_t elementId)
	{
		return aliveIds[elementId];
	}
	size_t GetElementsCount()
	{
		return aliveIds.size();
	}
	size_t GetAllocatedCount()
	{
		return elements.size();
	}
	size_t Add(bool &isCreated)
	{
		this->contentsChanged = true;
		if (freeIds.size() > 0)
		{
			size_t freeElementId = freeIds.back();
			freeIds.pop_back();
			aliveIds.push_back(freeElementId);
			states[freeElementId] = State::Alive;
			isCreated = false;
			return freeElementId;
		}
		else
		{
			aliveIds.push_back(elements.size());
			elements.push_back(T());
			states.push_back(State::Alive);
			isCreated = true;
			return elements.size() - 1;
		}
	}
	size_t Add()
	{
		bool isCreated;
		return Add(isCreated);
	}

	size_t Add(T &&newbie)
	{
		bool isCreated;
		return Add(std::move(newbie), isCreated);
	}
	size_t Add(T &&newbie, bool &isCreated)
	{
		this->contentsChanged = true;
		if(freeIds.size() > 0)
		{
			size_t freeElementId = freeIds.back();
			freeIds.pop_back();
			aliveIds.push_back(freeElementId);
			states[freeElementId] = State::Alive;
			elements[freeElementId] = std::move(newbie);
			isCreated = false;
			return freeElementId;
		}else
		{
			aliveIds.push_back(elements.size());
			elements.emplace_back(std::move(newbie));
			states.push_back(State::Alive);
			isCreated = true;
			return elements.size() - 1;
		}
	}
	void RemoveById(size_t elementId)
	{
		this->contentsChanged = true;
		if (states[elementId] == State::Alive)
			states[elementId] = State::Releasing;
	}
	void RemoveByIndex(size_t elementIndex)
	{
		this->contentsChanged = true;
		if(states[aliveIds[elementIndex]] == State::Alive)
			states[aliveIds[elementIndex]] = State::Releasing;
	}
	bool IsIdAlive(size_t elementId)
	{
		if (elementId < states.size())
		{
			return states[elementId] == State::Alive;
		}
		else
			return false;
	}
	bool IsIndexAlive(size_t elementIndex)
	{
		if (elementIndex < aliveIds.size())
		{
			return states[aliveIds[elementIndex]] == State::Alive;
		}
		else
			return false;
	}
	void Update()
	{
		if(!contentsChanged) return;
		contentsChanged = false;

		aliveIds.clear();
		for(size_t elementId = 0; elementId < elements.size(); elementId++)
		{
			if(states[elementId] == State::Releasing)
			{
				elements[elementId] = T();
				states[elementId] = State::Dead;
				freeIds.push_back(elementId);
			}
			if(states[elementId] == State::Alive)
			{
				aliveIds.push_back(elementId);
			}
		}
	}
private:
	struct State
	{
		enum Types
		{
			Alive,
			Releasing,
			Dead
		};
	};
	std::vector<size_t> freeIds;
	std::vector<size_t> aliveIds;
	std::vector<typename State::Types> states;
	std::vector<T> elements;
	bool contentsChanged;
};

template<typename T>
struct LinearCachedArray
{
public:
	LinearCachedArray()
	{
		this->contentsChanged = true;
	}
	T &GetById(size_t elementId)
	{
		return elements[idToIndex[elementId]];
	}
	T &GetByIndex(size_t elementIndex)
	{
		return elements[elementIndex];
	}
	const T &GetByIndex(size_t elementIndex) const
	{
		return elements[elementIndex];
	}
	T *GetElements()
	{
		return elements.data();
	}
	size_t GetElementsCount() const
	{
		return elements.size();
	}
	size_t GetId(size_t elementIndex)
	{
		return indexToId[elementIndex];
	}
	size_t GetIndex(size_t elementId)
	{
		return idToIndex[elementId];
	}

	size_t GetAllocatedCount()
	{
		return elements.capacity();
	}

	size_t Add(bool &isCreated)
	{
		return Add(std::move(T()), isCreated);
	}
	size_t Add()
	{
		bool isCreated;
		return Add(std::move(T()), isCreated);
	}
	size_t Add(T &&newbie)
	{
		bool isCreated;
		return Add(std::move(newbie), isCreated);
	}
	size_t Add(T &&newbie, bool &isCreated)
	{
		this->contentsChanged = true;
		size_t newId = size_t(-1);
		if (freeIds.size() > 0)
		{
			newId = freeIds.back();
			freeIds.pop_back();

			idToIndex[newId] = elements.size();

			isCreated = false;
		}
		else
		{
			newId = elements.size();
			size_t newIndex = elements.size();

			assert(idToIndex.size() == elements.size());
			idToIndex.push_back(newIndex);

			isCreated = true;
		}
		assert(indexToId.size() == elements.size());
		indexToId.push_back(newId);

		assert(states.size() == elements.size());
		states.push_back(State::Alive);

		elements.push_back(std::move(newbie));
		return newId;
	}
	void RemoveById(size_t elementId)
	{
		this->contentsChanged = true;
		if (states[idToIndex[elementId]] == State::Alive)
			states[idToIndex[elementId]] = State::Releasing;
	}
	void RemoveByIndex(size_t elementIndex)
	{
		this->contentsChanged = true;
		if (states[elementIndex] == State::Alive)
			states[elementIndex] = State::Releasing;
	}
	bool IsIdAlive(size_t elementId)
	{
		if (elementId < idToIndex.size())
		{
			return states[idToIndex[elementId]] == State::Alive;
		}
		else
			return false;
	}
	bool IsIndexAlive(size_t elementIndex)
	{
		if (elementIndex < states.size())
		{
			return states[elementIndex] == State::Alive;
		}
		else
			return false;
	}
	void Update()
	{
		if(!contentsChanged) return;
		contentsChanged = false;

		for (size_t elementIndex = 0; elementIndex < elements.size();)
		{
			if (states[elementIndex] == State::Releasing)
			{
				size_t lastIndex = elements.size() - 1;
				size_t lastId = indexToId[lastIndex];

				size_t elementId = indexToId[elementIndex];

				freeIds.push_back(elementId);

				indexToId[lastIndex] = -1;
				idToIndex[lastId] = elementIndex;
				indexToId[elementIndex] = lastId;
				idToIndex[elementId] = -1;

				states[elementIndex] = states[lastIndex];
				states[lastIndex] = State::Dead;

				//elements[elementIndex].~T();
				elements[elementIndex] = std::move(elements[lastIndex]);

				indexToId.pop_back();
				states.pop_back();
				elements.pop_back();
			}
			else
			{
				elementIndex++;
			}
		}
	}
private:
	struct State
	{
		enum Types
		{
			Alive,
			Releasing,
			Dead
		};
	};
	std::vector<size_t> freeIds;
	std::vector<size_t> idToIndex;
	std::vector<size_t> indexToId;
	std::vector<typename State::Types> states;
	std::vector<T> elements;
	bool contentsChanged;
};