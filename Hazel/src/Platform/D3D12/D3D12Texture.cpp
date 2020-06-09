#include "hzpch.h"

#include "Hazel/Core/Application.h"

#include "Platform/D3D12/D3D12Texture.h"
#include "Platform/D3D12/D3D12Context.h"
#include "Platform/D3D12/DDSTextureLoader/DDSTextureLoader.h"


namespace Hazel {

	D3D12Texture2D::D3D12Texture2D(std::wstring id, uint32_t width, uint32_t height, uint32_t mips)
		: m_Identifier(id),
		m_Width(width),
		m_Height(height),
		m_MipLevels(mips),
		m_CurrentState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	{
	}
	
	void D3D12Texture2D::SetData(D3D12ResourceUploadBatch& batch, void* data, uint32_t size)
	{
		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = data;
		subresourceData.RowPitch = (uint64_t)m_Width * 4 * sizeof(uint8_t);
		subresourceData.SlicePitch = subresourceData.RowPitch * m_Height;

		batch.Upload(m_Resource.Get(), 0, &subresourceData, 1);
	}

	void D3D12Texture2D::Transition(D3D12ResourceUploadBatch& batch, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
	{
		batch.GetCommandList()->ResourceBarrier(1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				m_Resource.Get(),
				from,
				to
			)
		);
		m_CurrentState = to;
	}

	void D3D12Texture2D::Transition(D3D12ResourceUploadBatch& batch, D3D12_RESOURCE_STATES to)
	{
		if (to == m_CurrentState)
			return;
		this->Transition(batch, m_CurrentState, to);
	}

	void D3D12Texture2D::Transition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
	{
		commandList->ResourceBarrier(1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				m_Resource.Get(),
				from,
				to
			)
		);
		m_CurrentState = to;
	}

	void D3D12Texture2D::Transition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES to)
	{
		if (to == m_CurrentState)
			return;
		this->Transition(commandList, m_CurrentState, to);
	}
	
	Ref<D3D12Texture2D> D3D12Texture2D::CreateVirtualTexture(D3D12ResourceUploadBatch& batch, TextureCreationOptions& opts)
	{
		if (!opts.Path.empty() && opts.Name.empty())
		{
			HZ_CORE_ASSERT(false, "Virtual textures cannot have a path, but they need a name");
		}

		Ref<D3D12VirtualTexture2D> ret = CreateRef<D3D12VirtualTexture2D>(opts.Name, opts.Width, opts.Height, opts.MipLevels);
		auto device = batch.GetDevice();

		D3D12_RESOURCE_DESC desc = {};
		desc.MipLevels = opts.MipLevels;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Width = opts.Width;
		desc.Height = opts.Height;
		// D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		desc.Flags = opts.Flags;
		desc.DepthOrArraySize = 1;
		desc.SampleDesc = { 1, 0 };
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
		
		D3D12::ThrowIfFailed(device->CreateReservedResource(
			&desc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(ret->m_Resource.GetAddressOf())
		));

		ret->m_Resource->SetName(opts.Name.c_str());

		UINT numTiles = 0;
		D3D12_TILE_SHAPE tileShape = {};
		UINT subresourceCount = desc.MipLevels;
		D3D12_PACKED_MIP_INFO mipInfo;
		std::vector<D3D12_SUBRESOURCE_TILING> tilings(subresourceCount);

		batch.GetDevice()->GetResourceTiling(
			ret->m_Resource.Get(), &numTiles, &mipInfo,
			&tileShape, &subresourceCount, 0, &tilings[0]);

		ret->m_TileAllocations.resize(tilings.size());

		for (uint32_t i = 0; i < tilings.size(); i++)
		{
			auto& allocationVector = ret->m_TileAllocations[i];
			auto& dims = tilings[i];

			allocationVector.resize(dims.WidthInTiles * dims.HeightInTiles);

			for (uint32_t y = 0; y < dims.HeightInTiles; ++y)
			{
				for (uint32_t x = 0; x < dims.WidthInTiles; ++x)
				{
					uint32_t index =(y * dims.WidthInTiles + x);
					allocationVector[index].Mapped = false;
					allocationVector[index].ResourceCoordinate = CD3DX12_TILED_RESOURCE_COORDINATE(x, y, 0, i);
				}
			}

		}

		return ret;
	}

	Ref<D3D12Texture2D> D3D12Texture2D::CreateCommittedTexture(D3D12ResourceUploadBatch& batch, TextureCreationOptions& opts)
	{
		Ref<D3D12Texture2D> ret = nullptr;

		// We need to load from a file
		if (!opts.Path.empty())
		{
			TComPtr<ID3D12Resource> resource = nullptr;
			std::unique_ptr<uint8_t[]> ddsData = nullptr;
			bool isCube;
			DirectX::DDS_ALPHA_MODE alphaMode;
			std::vector<D3D12_SUBRESOURCE_DATA> subData;

			// Leaves resource in COPY_DEST state
			DirectX::LoadDDSTextureFromFile(
				batch.GetDevice().Get(),
				opts.Path.c_str(),
				resource.GetAddressOf(),
				ddsData,
				subData,
				0,
				&alphaMode,
				&isCube
			);

			auto desc = resource->GetDesc();
			ret = CreateRef<D3D12CommittedTexture2D>(
				opts.Path,
				desc.Width,
				desc.Height,
				desc.MipLevels
			);
			ret->m_Resource.Swap(resource);
			ret->m_CurrentState = D3D12_RESOURCE_STATE_COPY_DEST;
			batch.Upload(ret->m_Resource.Get(), 0, subData.data(), subData.size());
		}
		// We create an in memory texture
		else if (!opts.Name.empty())
		{
			ret = CreateRef<D3D12CommittedTexture2D>(opts.Name, opts.Width, opts.Height, opts.MipLevels);

			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.MipLevels = opts.MipLevels;
			textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			textureDesc.Width = opts.Width;
			textureDesc.Height = opts.Height;
			textureDesc.Flags = opts.Flags;
			textureDesc.DepthOrArraySize = 1;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			Hazel::D3D12::ThrowIfFailed(batch.GetDevice()->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
				&textureDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				IID_PPV_ARGS(ret->m_Resource.GetAddressOf())
			));
		}
		else
		{
			HZ_CORE_ASSERT(false, "Commited textures need to have a path or a name");
		}
		return ret;
	}

	Ref<D3D12FeedbackMap> D3D12Texture2D::CreateFeedbackMap(D3D12ResourceUploadBatch& batch, Ref<D3D12Texture2D> texture)
	{
		auto resource = texture->GetResource();
		auto desc = resource->GetDesc();
		uint32_t tiles_x = 1;
		uint32_t tiles_y = 1;

		if (texture->IsVirtual()) 
		{
			// We need to get tiling info from the device.
			UINT numTiles = 0;
			D3D12_TILE_SHAPE tileShape = {};
			UINT subresourceCount = texture->GetMipLevels();
			D3D12_PACKED_MIP_INFO mipInfo;
			std::vector<D3D12_SUBRESOURCE_TILING> tilings(subresourceCount);

			batch.GetDevice()->GetResourceTiling(
				resource, &numTiles, &mipInfo,
				&tileShape, &subresourceCount, 0, &tilings[0]);

			tiles_x = tilings[0].WidthInTiles;
			tiles_y = tilings[0].HeightInTiles;
		}
		else 
		{
			// D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT = 64K
			uint32_t factor = (desc.Height * desc.Width) / D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			tiles_x = desc.Width / factor;
			tiles_y = desc.Height / factor;
		}

		// TODO: The feedback map is hardcoded to always be uint32_t now. Might have to change it later.
		// The constructor already supports it.
		auto ret = CreateRef<D3D12FeedbackMap>(
						batch.GetDevice(),
						tiles_x, tiles_y, sizeof(uint32_t)
					);


		return ret;
	}

	/**
	*
	*       Virtual Texture
	*
	*/
	D3D12VirtualTexture2D::D3D12VirtualTexture2D(std::wstring id, uint32_t width, uint32_t height, uint32_t mips)
		: D3D12Texture2D(id, width, height, mips)
	{
	}

	/**
	*
	*       Committed Texture
	*
	*/
	D3D12CommittedTexture2D::D3D12CommittedTexture2D(std::wstring id, uint32_t width, uint32_t height, uint32_t mips)
		: D3D12Texture2D(id, width, height, mips)
	{
	}
}

