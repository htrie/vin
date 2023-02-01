namespace Physics
{
	template<typename HandleInfo>
	SimpleUniqueHandle<HandleInfo>::SimpleUniqueHandle(HandleInfo handleInfo, bool isWeakHandle)
	{
		this->handleInfo = handleInfo;
		this->isWeakHandle = isWeakHandle;
	}
	template<typename HandleInfo>
	SimpleUniqueHandle<HandleInfo>::~SimpleUniqueHandle()
	{
		if(!isWeakHandle)
			this->Release();
	}
	template<typename HandleInfo>
	void SimpleUniqueHandle<HandleInfo>::Release()
	{
		assert(!isWeakHandle);
		if(IsValid())
		{
			handleInfo.Release();
			handleInfo = HandleInfo();
		}
	}
	template<typename HandleInfo>
	typename HandleInfo::ObjectType* SimpleUniqueHandle<HandleInfo>::Get()
	{
		return IsValid() ? handleInfo.Get() : nullptr;
	}

	template<typename HandleInfo>
	bool SimpleUniqueHandle<HandleInfo>::IsValid() const
	{
		return /*!(handleInfo == HandleInfo());*/handleInfo.IsValid();
	}

	template<typename HandleInfo>
	void SimpleUniqueHandle<HandleInfo>::Detach()
	{
		handleInfo.Detach();
		handleInfo = HandleInfo();
	}

	template<typename HandleInfo>
	SimpleUniqueHandle<HandleInfo>::SimpleUniqueHandle()
	{
		handleInfo = HandleInfo();
		this->isWeakHandle = true;
	}
	template<typename HandleInfo>
	SimpleUniqueHandle<HandleInfo>::SimpleUniqueHandle(SimpleUniqueHandle<HandleInfo> && other)
	{
		this->handleInfo = other.handleInfo;
		this->isWeakHandle = other.isWeakHandle;
		other.handleInfo = HandleInfo();
	}
	template<typename HandleInfo>
	SimpleUniqueHandle<HandleInfo> & SimpleUniqueHandle<HandleInfo>::operator=(SimpleUniqueHandle<HandleInfo> && other)
	{
		if(!isWeakHandle)
		{
			this->Release();
		}
		this->handleInfo = other.handleInfo;
		this->isWeakHandle = other.isWeakHandle;
		other.handleInfo = HandleInfo();
		return *this;
	}
}