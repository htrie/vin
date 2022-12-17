#pragma once

namespace Device
{
	class VertexBuffer;
	class IndexBuffer;
	enum class Lock : unsigned;
}

namespace Utility
{

	///Locks a vertex or index buffer to allow access to its elements.
	template< typename BufferType, typename ElementType >
	class LockBuffer
	{
	public:
		///@param size in elements.
		LockBuffer( Device::Handle<BufferType > buffer, const unsigned size, const Device::Lock lock );
		LockBuffer( Device::Handle<BufferType > buffer, const unsigned start, const unsigned size, const Device::Lock lock );
		~LockBuffer() { buffer->Unlock(); }

		ElementType* begin() { return _data; } // TODO: Check access bounds.
		ElementType* end() { return _data + size; } // TODO: Check access bounds.

		ElementType* data() { return _data; }

	private:
		Device::Handle<BufferType > buffer;
		ElementType* _data = nullptr;
		const unsigned size;
	};

	template< typename Func, typename IndexType >
	void ForEachTriangleInBuffer( const uint8_t* vertex_data, const size_t vertex_stride, const IndexType* indices, const size_t base_index, const size_t num_tris, Func f )
	{
		for( size_t i = 0; i < num_tris; ++i )
		{
			const auto& a = *reinterpret_cast<const simd::vector3*>( vertex_data + indices[ i * 3 + 0 + base_index ] * vertex_stride );
			const auto& b = *reinterpret_cast<const simd::vector3*>( vertex_data + indices[ i * 3 + 1 + base_index ] * vertex_stride );
			const auto& c = *reinterpret_cast<const simd::vector3*>( vertex_data + indices[ i * 3 + 2 + base_index ] * vertex_stride );
			f( a, b, c );

			if constexpr( std::is_same< std::result_of_t< Func( simd::vector3, simd::vector3, simd::vector3 )>, bool >::value )
			{
				if( f( a, b, c ) )
					return;
			}
			else
			{
				f( a, b, c );
			}
		}
	}

	///Iterates through triangles in a vertex / index buffer pair calling the function with the vertices.
	template< typename Func >
	void ForEachTriangleInBuffer( const uint8_t* vertex_data, Device::Handle< Device::IndexBuffer > indices, const size_t base_index, const size_t num_tris, const size_t stride, Func f )
	{
		Device::IndexBufferDesc ib_desc;
		indices->GetDesc( &ib_desc );
		assert( ( ib_desc.Format == Device::PixelFormat::INDEX16 ) || ( ib_desc.Format == Device::PixelFormat::INDEX32 ) );

		if( ib_desc.Format == Device::PixelFormat::INDEX16 )
		{
			LockBuffer< Device::IndexBuffer, uint16_t > index_buffer( indices, static_cast< unsigned >( base_index ), static_cast< unsigned >( num_tris ), Device::Lock::READONLY );
			ForEachTriangleInBuffer( vertex_data, stride, index_buffer.data(), base_index, num_tris, f );
		}
		else
		{
			LockBuffer< Device::IndexBuffer, uint32_t > index_buffer( indices, static_cast< unsigned >( base_index ), static_cast< unsigned >( num_tris ), Device::Lock::READONLY );
			ForEachTriangleInBuffer( vertex_data, stride, index_buffer.data(), base_index, num_tris, f );
		}
	}

	template< typename Func >
	void ForEachTriangleInBuffer( Device::Handle< Device::VertexBuffer > vertices, Device::Handle< Device::IndexBuffer > indices, const size_t base_index, const size_t num_tris, const size_t stride, Func f )
	{
		LockBuffer< Device::VertexBuffer, uint8_t > vertex_buffer( vertices, 0, 0, Device::Lock::READONLY );
		ForEachTriangleInBuffer( vertex_buffer.data(), indices, base_index, num_tris, stride, f );
	}

	inline std::vector<uint8_t> ReadVertices( Device::Handle< Device::VertexBuffer > vertex_buffer )
	{
		uint8_t* vertex_data = nullptr;
		vertex_buffer->Lock( 0, 0, reinterpret_cast<void**>( &vertex_data ), Device::Lock::READONLY );
		std::vector<uint8_t> vertices;
		vertices.resize(vertex_buffer->GetSize());
		memcpy(vertices.data(), vertex_data, vertex_buffer->GetSize());
		vertex_buffer->Unlock();
		return vertices;
	}

	template<typename Index>
	std::vector<Index> ReadIndices( Device::Handle< Device::IndexBuffer > index_buffer )
	{
		Device::IndexBufferDesc ib_desc;
		index_buffer->GetDesc( &ib_desc );
		assert( ( ib_desc.Format == Device::PixelFormat::INDEX16 ) || ( ib_desc.Format == Device::PixelFormat::INDEX32 ) );
		const size_t index_size = ( ( ib_desc.Format == Device::PixelFormat::INDEX16 ) ? sizeof( unsigned short ) : sizeof( unsigned int ) );
		const unsigned num_indices = static_cast< unsigned >( ib_desc.Size / index_size );
		assert( num_indices % 3 == 0 );
		const unsigned num_tris = num_indices / 3;

		uint8_t* index_data = nullptr;
		index_buffer->Lock( 0, 0, reinterpret_cast<void**>( &index_data ), Device::Lock::READONLY );

		std::vector<Index> res_indices;
		if( ib_desc.Format == Device::PixelFormat::INDEX16 )
		{
			auto* indices = (uint16_t*)index_data;
			for( size_t i = 0; i < num_indices; ++i )
			{
				res_indices.push_back( indices[ i ] );
			}
		}
		else
		{
			auto* indices = (uint32_t*)index_data;
			for( size_t i = 0; i < num_tris; ++i )
			{
				res_indices.push_back( indices[ i ] );
			}
		}
		return res_indices;
	}

	////////////// Template function implementation ///////////////////

	template< typename BufferType, typename ElementType >
	Utility::LockBuffer<BufferType, ElementType>::LockBuffer( Device::Handle<BufferType > buffer, const unsigned size, const Device::Lock lock )
		: size( size ), buffer( buffer )
	{
		buffer->Lock( 0, size * sizeof( ElementType ), (void**)&_data, lock );
	}

	template< typename BufferType, typename ElementType >
	Utility::LockBuffer<BufferType, ElementType>::LockBuffer( Device::Handle<BufferType > buffer, const unsigned start, const unsigned size, const Device::Lock lock )
		: size( size ), buffer( buffer )
	{
		buffer->Lock( start * sizeof( ElementType ), size * sizeof( ElementType ), (void**)&_data, lock );
	}
}