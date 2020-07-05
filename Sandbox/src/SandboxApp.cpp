#include <Hazel.h>
#include <Hazel/Core/EntryPoint.h>

#include "Sandbox2D.h"
#include "ExampleLayer.h"
#include "BenchmarkLayer.h"

class Sandbox : public Hazel::Application
{
public:
	Sandbox(): Hazel::Application(Hazel::RendererAPI::API::D3D12)
	{
		//PushLayer(new ExampleLayer());

		PushLayer(new BenchmarkLayer());
	}

	~Sandbox()
	{
	}
};

Hazel::Application* Hazel::CreateApplication()
{
	return new Sandbox();
}
