#pragma once
namespace Physics
{
	template<typename HandleInfo>
	class SimpleUniqueHandle
	{
	public:
		typename HandleInfo::ObjectType *Get();
		bool IsValid() const;
		void Detach();
		SimpleUniqueHandle();
		virtual ~SimpleUniqueHandle();
		SimpleUniqueHandle(SimpleUniqueHandle &&other);
		SimpleUniqueHandle &operator =(SimpleUniqueHandle &&other);

		SimpleUniqueHandle(HandleInfo handleInfo, bool isWeakHandle);
	protected:
		HandleInfo handleInfo;
	private:
		void Release();
		SimpleUniqueHandle(SimpleUniqueHandle &) = delete;
		SimpleUniqueHandle &operator =(SimpleUniqueHandle &) = delete;
		bool isWeakHandle;
	};
}
#include "Handles.inl"