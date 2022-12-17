#pragma once

namespace Device
{

	class ConstantBufferD3D11
	{
	public:
		void Clear();
		void Reset();

		class Buffer
		{
		public:
			Buffer(ID3D11Device* device, size_t max_size);

			void Reset();

			void Lock(ID3D11DeviceContext* context, size_t size, ID3D11Buffer*& out_buffer, uint8_t*& out_map);
			void Unlock(ID3D11DeviceContext* context);

			bool HasSpace(size_t aligned_size) const;
			uint8_t* Allocate(ID3D11DeviceContext* context, size_t aligned_size, ID3D11Buffer*& out_buffer, UINT& out_constant_offset, UINT& out_constant_count);
			void Finalize(ID3D11DeviceContext* context);

		private:
			void Map(ID3D11DeviceContext* context);
			void Unmap(ID3D11DeviceContext* context);

			std::unique_ptr<ID3D11Buffer, Utility::GraphicsComReleaser> buffer;
			D3D11_MAPPED_SUBRESOURCE mapped_resource;
			size_t offset;
			bool is_mapped;
			size_t max_size;
			unsigned bucket_index;
		};

		Buffer& Lock(ID3D11Device* device, ID3D11DeviceContext* context, size_t size, ID3D11Buffer*& out_buffer, uint8_t*& out_map);
		void Unlock(ID3D11DeviceContext* context, Buffer& buffer);

		uint8_t* Allocate(ID3D11Device* device, ID3D11DeviceContext* context, size_t size, ID3D11Buffer*& out_buffer, UINT& out_constant_offset, UINT& out_constant_count);
		void Finalize(ID3D11DeviceContext* context);

	private:

		static unsigned ComputeBucketIndex(size_t size);
		static size_t ComputeBucketSize(size_t size, size_t max_size);

		static const unsigned BucketCount = 4;

		std::array<std::deque<std::shared_ptr<Buffer>>, BucketCount> available_buffers_buckets;
		std::array<std::deque<std::shared_ptr<Buffer>>, BucketCount> used_buffers_buckets;

		Buffer& Grab(ID3D11Device* device, size_t size, size_t max_size);
	};

}
