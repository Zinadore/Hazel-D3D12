#pragma once

#include "Hazel/Renderer/Vertex.h"

#include "Platform/D3D12/D3D12Buffer.h"

namespace Hazel {
	struct HMesh
	{
		Hazel::Ref<Hazel::D3D12VertexBuffer> vertexBuffer;
		Hazel::Ref<Hazel::D3D12IndexBuffer> indexBuffer;

		std::vector<Hazel::Vertex> vertices;
		std::vector<uint32_t> indices;
	};
}
