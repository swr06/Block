/* -- The Blocks Voxel Engine -- 
* 
Contributors : 
Samuel Rasquinha 
Dekrain
Kiroma
ShadaxStack (fuzdex)
g h o s t
UglySwedishFish (WorldTeller)
*/


#include <stdio.h>
#include <iostream>
#include <array>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <memory>

#include <glad/glad.h>          
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "Core/Application/Application.h"
#include "Core/GLClasses/VertexBuffer.h"
#include "Core/GLClasses/VertexArray.h"
#include "Core/GLClasses/Shader.h"
#include "Core/GLClasses/Fps.h"
#include "Core/FpsCamera.h"
#include "Core/CubeRenderer.h"
#include "Core/Chunk.h"
#include "Core/ChunkMesh.h"
#include "Core/World.h"
#include "Core/BlockDatabaseParser.h"
#include "Core/BlockDatabase.h"
#include "Core/AtmosphereRenderer.h"
#include "Core/OrthographicCamera.h"
#include "Core/Renderer2D.h"
#include "Core/Player.h"
#include "Core/GLClasses/DepthBuffer.h"
#include "Core/ShadowRenderer.h"
#include "Core/BlocksRenderBuffer.h"
#include "Core/GLClasses/Framebuffer.h"
#include "Core/GLClasses/FramebufferRed.h"
#include "Core/CubemapReflectionRenderer.h"
#include "Core/Utils/Timer.h"
#include "Core/BloomRenderer.h"

// World, Camera, Player..
Blocks::Player Player;
Blocks::OrthographicCamera OCamera(0.0f, 800.0f, 0.0f, 600.0f);
Blocks::World MainWorld;

// Framebuffers, Renderbuffers and depth buffers
Blocks::BlocksRenderBuffer MainRenderFBO(800, 600);
Blocks::BlocksRenderBuffer SecondaryRenderFBO(800, 600);
GLClasses::Framebuffer TempFBO(800, 600, false, true);

GLClasses::FramebufferRed VolumetricLightingFBO(800, 600);
GLClasses::Framebuffer SSRFBO(800, 600, true);
GLClasses::Framebuffer RefractionFBO(800, 600, true);
GLClasses::Framebuffer BloomFBO(133, 100, true); // 1/6th resolution

// Flags 
bool ShouldRenderSkybox = true;
bool ShouldRenderVolumetrics = false;
bool ShouldDoBloomPass = true;
bool ShouldDoSSRPass = false;
bool ShouldDoRefractions = true;
bool ShouldDoWaterParallax = false;

bool _Bloom = true;
bool _SSR = true;

// Flags that change from frame to frame
bool PlayerMoved = false;
bool BlockModified = false;
bool SunDirectionChanged = false;

// Tweakable variables
glm::vec3 SunDirection = glm::vec3(0.0f);
glm::vec3 MoonDirection = glm::vec3(0.0f);

// ImGui Adjustable Variables
float ShadowBias = 0.001f;
float VolumetricScattering = 0.6f;
float RenderScale = 1.0f;
float VolumetricRenderScale = 0.5f;
float SSRRenderScale = 0.25f;
float WaterParallaxDepth = 8.0f;
float WaterParallaxHeight = 0.5f;
float Exposure = 3.25f;
float AtmosphereRenderScale = 0.04;

bool DepthPrePass = false;
bool VSync = true;

bool TickSun = true;

int AtmosphereLightSteps = 2;
int AtmosphereSteps = 30;

float FakeSunTime = 0.0f;

struct RenderingTime
{
	float SSR;
	float Refractions;
	float CubemapReflection;
	float ShadowMap;
	float Rendering;
	float Water;
	float PostProcessing;
	float Bloom;
	float Volumetrics;
	float Update;
	float Atmosphere;
	float TotalMeasured;
};

extern uint32_t _App_PolygonCount; // Internal! Do not touch.
RenderingTime AppRenderingTime;

class BlocksApp : public Blocks::Application
{
public:

	BlocksApp()
	{
		m_Width = 800;
		m_Height = 600;
		m_Appname = "Blocks";
	}

	void OnUserCreate(double ts) override
	{

	}

	void OnUserUpdate(double ts) override
	{

	}

	void OnImguiRender(double ts) override
	{
		ImGuiWindowFlags window_flags = 0;

		glm::vec3 prevSunDirection = SunDirection;

		if (ImGui::Begin("Stats"))
		{
			ImGui::Text("Polygon Count : %d", _App_PolygonCount);
			ImGui::Text("Position : (%f, %f, %f)", Player.Camera.GetPosition().x, Player.Camera.GetPosition().y, Player.Camera.GetPosition().z);
			ImGui::Text("Player.Camera Direction : (%f, %f, %f)", Player.Camera.GetFront().x, Player.Camera.GetFront().y, Player.Camera.GetFront().z);
			ImGui::Text("Sun Direction : (%f, %f, %f)", SunDirection.x, SunDirection.y, SunDirection.z);
			ImGui::Text("Moon Direction : (%f, %f, %f)", MoonDirection.x, MoonDirection.y, MoonDirection.z);
			ImGui::End();
		}

		if (ImGui::Begin("Settings"))
		{
			ImGui::Checkbox("Tick Sun", &TickSun);
			ImGui::SliderFloat("Sun Angle", &FakeSunTime, 0.0f, 64.0f);
			ImGui::SliderFloat("Shadow Bias", &ShadowBias, 0.001f, 0.05f, 0);
			ImGui::SliderFloat("Volumetric Scattering", &VolumetricScattering, 0.0f, 1.0f);
			ImGui::SliderFloat("Exposure", &Exposure, 0.5f, 10.0f);
			ImGui::Checkbox("Depth Prepass? (Reduces overdraw)", &DepthPrePass);
			ImGui::Text("\n");
			ImGui::Checkbox("Render Skybox?", &ShouldRenderSkybox);
			ImGui::Checkbox("Render Volumetrics?", &ShouldRenderVolumetrics);
			ImGui::Checkbox("Bloom?", &ShouldDoBloomPass);
			ImGui::Checkbox("SSR Pass?", &ShouldDoSSRPass);
			ImGui::Checkbox("SS Refractions?", &ShouldDoRefractions);
			ImGui::Checkbox("Freefly (Shouldn't do collisions) ?", &Player.Freefly);
			ImGui::SliderFloat("Render Scale", &RenderScale, 0.1f, 1.5f);
			ImGui::SliderFloat("Volumetric Render Resolution", &VolumetricRenderScale, 0.1f, 1.1f);
			ImGui::SliderFloat("SSR Render Resolution", &SSRRenderScale, 0.1f, 1.1f);
			ImGui::Checkbox("Water Parallax?", &ShouldDoWaterParallax);
			ImGui::SliderFloat("Water Parallax Depth", &WaterParallaxDepth, 8.0f, 96.0f);
			ImGui::SliderFloat("Water Parallax Height", &WaterParallaxHeight, 0.25f, 4.0f);
			ImGui::SliderFloat("Atmosphere Render Scale", &AtmosphereRenderScale, 0.03f, 0.4f);
			ImGui::SliderInt("Atmosphere Sample Count", &AtmosphereSteps, 2, 100);
			ImGui::SliderInt("Atmosphere Light Sample Count", &AtmosphereLightSteps, 2, 40);
			ImGui::End();
		}

		static RenderingTime render_time;

		if (this->GetCurrentFrame() % 8 == 0)
		{
			render_time = AppRenderingTime;

			// Write the default time values
			AppRenderingTime.SSR = 0.0f;
			AppRenderingTime.Refractions = 0.0f;
			AppRenderingTime.CubemapReflection = 0.0f;
			AppRenderingTime.ShadowMap = 0.0f;
			AppRenderingTime.Rendering = 0.0f;
			AppRenderingTime.Water = 0.0f;
			AppRenderingTime.PostProcessing = 0.0f;
			AppRenderingTime.Bloom = 0.0f;
			AppRenderingTime.Volumetrics = 0.0f;
			AppRenderingTime.Atmosphere = 0.0f;
			AppRenderingTime.TotalMeasured = 0.0f;
		}

		if (ImGui::Begin("Performance"))
		{
			ImGui::Text("SSR Render time : %f", render_time.SSR);
			ImGui::Text("SS Refraction Render time : %f", render_time.Refractions);
			ImGui::Text("Cubemap Reflection Map Render time : %f", render_time.CubemapReflection);
			ImGui::Text("Shadow Map Render time : %f", render_time.ShadowMap);
			ImGui::Text("Render time : %f", render_time.Rendering);
			ImGui::Text("Water Render time : %f", render_time.Water);
			ImGui::Text("Bloom time : %f", render_time.Bloom);
			ImGui::Text("Volumetrics time : %f", render_time.Volumetrics);
			ImGui::Text("Post Processing time : %f", render_time.PostProcessing);
			ImGui::Text("Update time : %f", render_time.Update);
			ImGui::Text("Atmospherics time : %f", render_time.Atmosphere);
			ImGui::Text("Total Measured time : %f", render_time.TotalMeasured);
			ImGui::Text("Total time : %f",
				render_time.SSR +
				render_time.Refractions +
				render_time.CubemapReflection +
				render_time.ShadowMap +
				render_time.Rendering +
				render_time.Water +
				render_time.PostProcessing +
				render_time.Bloom +
				render_time.Volumetrics + 
				render_time.Update
			);

			ImGui::End();
		}
		
		if (prevSunDirection != SunDirection)
		{
			SunDirectionChanged = true;
		}
	}

	void OnEvent(Blocks::Event e) override
	{
		if (e.type == Blocks::EventTypes::MouseMove && this->GetCursorLocked())
		{
			Player.Camera.UpdateOnMouseMovement(e.mx, e.my);
		}

		if (e.type == Blocks::EventTypes::WindowResize)
		{
			float aspect = (float)e.wx / (float)e.wy;

			Player.Camera.SetAspect(aspect);
			OCamera.SetProjection(0.0f, e.wx, 0.0f, e.wy);
			m_Width = e.wx;
			m_Height = e.wy;

			glViewport(0, 0, e.wx, e.wy);
		}

		if (e.type == Blocks::EventTypes::KeyPress && e.key == GLFW_KEY_F1)
		{
			this->SetCursorLocked(!this->GetCursorLocked());
		}

		if (e.type == Blocks::EventTypes::KeyPress && e.key == GLFW_KEY_Q)
		{
			MainWorld.ChangeCurrentBlock();
		}

		if (e.type == Blocks::EventTypes::KeyPress && e.key == GLFW_KEY_V)
		{
			VSync = !VSync;
		}

		if (e.type == Blocks::EventTypes::MousePress && e.button == GLFW_MOUSE_BUTTON_LEFT && this->GetCursorLocked())
		{
			BlockModified = true;
			MainWorld.RayCast(false, Player.Camera.GetPosition(), Player.Camera.GetFront());
		}

		if (e.type == Blocks::EventTypes::MousePress && e.button == GLFW_MOUSE_BUTTON_RIGHT && this->GetCursorLocked())
		{
			BlockModified = true;
			MainWorld.RayCast(true, Player.Camera.GetPosition(), Player.Camera.GetFront());
		}
	}
};


int main()
{
	BlocksApp app;

	app.Initialize();
	app.SetCursorLocked(true);

	Blocks::AtmosphereRenderer AtmosphereRenderer;
	Blocks::AtmosphereRenderMap AtmosphereCubemap(64);

	Blocks::BlockDatabase::Initialize();
	Blocks::Renderer2D Renderer2D;

	// Textures
	GLClasses::Texture Crosshair;
	GLClasses::Texture BlueNoiseTexture;
	GLClasses::Texture PerlinNoiseTexture;
	GLClasses::Texture PerlinNoiseNormalTexture;
	GLClasses::Texture WaterDetailNormalMap;
	GLClasses::Texture WaterMaps[100];

	// Shaders
	GLClasses::Shader RenderShader;
	GLClasses::Shader PPShader;
	GLClasses::Shader VolumetricShader;
	GLClasses::Shader BloomShader;
	GLClasses::Shader SSRShader;
	GLClasses::Shader WaterShader;
	GLClasses::Shader RefractionShader;
	GLClasses::Shader DepthPrepassShader;
	GLClasses::Shader AtmosphereCombineShader;

	GLClasses::VertexArray FBOVAO;
	GLClasses::VertexBuffer FBOVBO;
	GLClasses::DepthBuffer ShadowMap(4096, 4096);
	GLClasses::CubeReflectionMap ReflectionMap(256);

	Blocks::BloomFBO BloomFBO(800, 600);

	// Setup the basic vao

	float QuadVertices[] =
	{
		-1.0f,  1.0f,  0.0f, 1.0f, -1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 0.0f, -1.0f,  1.0f,  0.0f, 1.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,  1.0f,  1.0f,  1.0f, 1.0f
	};

	FBOVBO.BufferData(sizeof(QuadVertices), QuadVertices, GL_STATIC_DRAW);
	FBOVAO.Bind();
	FBOVBO.Bind();
	FBOVBO.VertexAttribPointer(0, 2, GL_FLOAT, 0, 4 * sizeof(GLfloat), 0);
	FBOVBO.VertexAttribPointer(1, 2, GL_FLOAT, 0, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
	FBOVAO.Unbind();

	// Create and compile the shaders
	RenderShader.CreateShaderProgramFromFile("Core/Shaders/BlockVert.glsl", "Core/Shaders/BlockFrag.glsl");
	RenderShader.CompileShaders();
	PPShader.CreateShaderProgramFromFile("Core/Shaders/Tonemapping/TonemapVert.glsl", "Core/Shaders/Tonemapping/ACES.glsl");
	PPShader.CompileShaders();
	VolumetricShader.CreateShaderProgramFromFile("Core/Shaders/VolumetricLightingVert.glsl", "Core/Shaders/VolumetricLightingFrag.glsl");
	VolumetricShader.CompileShaders();
	BloomShader.CreateShaderProgramFromFile("Core/Shaders/FBOVert.glsl", "Core/Shaders/BloomBrightFrag.glsl");
	BloomShader.CompileShaders();
	SSRShader.CreateShaderProgramFromFile("Core/Shaders/FBOVert.glsl", "Core/Shaders/SSRFrag.glsl");
	SSRShader.CompileShaders();
	RefractionShader.CreateShaderProgramFromFile("Core/Shaders/FBOVert.glsl", "Core/Shaders/SSRefractionsFrag.glsl");
	RefractionShader.CompileShaders();
	WaterShader.CreateShaderProgramFromFile("Core/Shaders/WaterVert.glsl", "Core/Shaders/WaterFrag.glsl");
	WaterShader.CompileShaders();
	DepthPrepassShader.CreateShaderProgramFromFile("Core/Shaders/DepthPrepassVert.glsl", "Core/Shaders/DepthPrepassFrag.glsl");
	DepthPrepassShader.CompileShaders();
	AtmosphereCombineShader.CreateShaderProgramFromFile("Core/Shaders/AtmosphereCombineVert.glsl", "Core/Shaders/AtmosphereCombineFrag.glsl");
	AtmosphereCombineShader.CompileShaders();

	// Create the texture
	Crosshair.CreateTexture("Res/crosshair.png", false);
	BlueNoiseTexture.CreateTexture("Res/Misc/blue_noise.png", false);
	PerlinNoiseTexture.CreateTexture("Res/Misc/perlin_noise.png", false);
	PerlinNoiseNormalTexture.CreateTexture("Res/Misc/perlin_noise_normal.png", false);
	WaterDetailNormalMap.CreateTexture("Res/Misc/water_detail_normal_map.png", false);

	// Create the water textures

	for (int i = 0; i <= 99; i++)
	{
		std::string name = "Res/Misc/water/Water_" + std::to_string(i) + ".png";
		WaterMaps[i].CreateTexture(name, false);
	}

	std::cout << "Created Water Textures!";

	// Set up the Orthographic Player.Camera
	OCamera.SetPosition(glm::vec3(0.0f));
	Player.Camera.SetPosition(glm::vec3(0.0f, 60.0f, 0.0f));

	// Setup shadow maps and shadow map renderer
	Blocks::ShadowMapRenderer::InitializeShadowRenderer();
	ShadowMap.Create();

	// Setup reflection map renderer
	Blocks::CubemapReflectionRenderer::Initialize();

	// Write default values
	SunDirection = glm::vec3(0.0f);

	Blocks::BloomRenderer::Initialize();

	while (!glfwWindowShouldClose(app.GetWindow()))
	{
		// Set MainRenderFBO Sizes

		if (TickSun)
		{
			SunDirection = glm::vec3(
				0.0f,
				glm::sin(glfwGetTime() * 0.1f),
				glm::cos(glfwGetTime() * 0.1f)
			);
		}

		else
		{
			SunDirection = glm::vec3(
				0.0f,
				glm::sin(FakeSunTime * 0.1f),
				glm::cos(FakeSunTime * 0.1f)
			);
		}

		MoonDirection = -SunDirection;
		glm::vec3 LightDirectionToUse = -SunDirection.y < 0.01f ? MoonDirection : SunDirection;

		float wx = app.GetWidth() * RenderScale, wy = app.GetHeight() * RenderScale;

		MainRenderFBO.SetDimensions(wx, wy);
		SecondaryRenderFBO.SetDimensions(wx, wy);
		TempFBO.SetSize(wx, wy);
		VolumetricLightingFBO.SetSize(floor((float)wx * VolumetricRenderScale), floor((float)wy * VolumetricRenderScale));
		SSRFBO.SetSize(wx * SSRRenderScale, wy * SSRRenderScale);
		RefractionFBO.SetSize(wx * 0.2f, wy * 0.2f);
		BloomFBO.SetSize(floor(wx), floor(wy));

		// ----------------- //


		Blocks::Timer total_timer;
		total_timer.Start();

		_SSR = ShouldDoSSRPass && (!Player.InWater);
		_Bloom = ShouldDoBloomPass && (!Player.InWater);

		Blocks::BlocksRenderBuffer& CurrentlyUsedFBO = (app.GetCurrentFrame() % 2 == 0) ? MainRenderFBO : SecondaryRenderFBO;
		Blocks::BlocksRenderBuffer& PreviousFrameFBO = (app.GetCurrentFrame() % 2 == 0) ? SecondaryRenderFBO : MainRenderFBO;

		glfwSwapInterval(VSync);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glEnable(GL_DEPTH_TEST);

		Blocks::Timer update_timer;

		update_timer.Start();
		app.OnUpdate();
		MainWorld.Update(Player.Camera.GetPosition(), Player.PlayerViewFrustum);
		PlayerMoved = Player.OnUpdate(app.GetWindow());

		AppRenderingTime.Update = update_timer.End();

		if (app.GetCurrentFrame() % 4 == 0)
		{
			Blocks::Timer shadow_timer;
			shadow_timer.Start();

			// Render the shadow map
			Blocks::ShadowMapRenderer::RenderShadowMap(ShadowMap, Player.Camera.GetPosition(), LightDirectionToUse, &MainWorld);
			AppRenderingTime.ShadowMap = shadow_timer.End();
		}

		if (app.GetCurrentFrame() % 10 == 0)
		{
			Blocks::Timer reflection_timer;
			reflection_timer.Start();

			// Render the reflection map
			Blocks::CubemapReflectionRenderer::Render(ReflectionMap, Player.Camera.GetPosition(), SunDirection, &MainWorld, nullptr);
			AppRenderingTime.CubemapReflection = reflection_timer.End();
		}

		// ---------
		// Atmospherics rendering pass

		Blocks::Timer atmosphere_timer;

		atmosphere_timer.Start();
		AtmosphereRenderer.RenderAtmosphere(AtmosphereCubemap, -SunDirection, AtmosphereSteps, AtmosphereLightSteps);
		AppRenderingTime.Atmosphere = atmosphere_timer.End();

		// ---------
		// SSR pass

		if (_SSR)
		{
			Blocks::Timer t1;
			t1.Start();

			SSRFBO.Bind();
			glViewport(0, 0, SSRFBO.GetWidth(), SSRFBO.GetHeight());

			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);

			SSRShader.Use();

			SSRShader.SetInteger("u_NormalTexture", 1);
			SSRShader.SetInteger("u_DepthTexture", 2);
			SSRShader.SetInteger("u_SSRMaskTexture", 3);
			SSRShader.SetInteger("u_NoiseTexture", 4);

			SSRShader.SetMatrix4("u_ProjectionMatrix", Player.Camera.GetProjectionMatrix());
			SSRShader.SetMatrix4("u_ViewMatrix", Player.Camera.GetViewMatrix());
			SSRShader.SetMatrix4("u_InverseProjectionMatrix", glm::inverse(Player.Camera.GetProjectionMatrix()));
			SSRShader.SetFloat("u_zNear", 0.1f);
			SSRShader.SetFloat("u_zFar", 1000.0f);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, PreviousFrameFBO.GetSSRNormalTexture());

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, PreviousFrameFBO.GetDepthTexture());

			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, PreviousFrameFBO.GetSSRMaskTexture());

			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, BlueNoiseTexture.GetTextureID());

			FBOVAO.Bind();
			glDrawArrays(GL_TRIANGLES, 0, 6);
			FBOVAO.Unbind();

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glUseProgram(0);

			AppRenderingTime.SSR = t1.End();
		}

		else
		{
			Blocks::Timer t1;
			t1.Start();

			SSRFBO.Bind();
			glViewport(0, 0, SSRFBO.GetWidth(), SSRFBO.GetHeight());

			glClearColor(-1.0f, -1.0f, -1.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glUseProgram(0);

			glClearColor(173.0f / 255.0f, 216.0f / 255.0f, 230.0f / 255.0f, 1.0f);
		
			AppRenderingTime.SSR = t1.End();
		}

		// ----------
		// Screen Space Refractions

		if (ShouldDoRefractions)
		{
			Blocks::Timer t2;
			t2.Start();

			RefractionFBO.Bind();
			glViewport(0, 0, RefractionFBO.GetWidth(), RefractionFBO.GetHeight());

			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);

			RefractionShader.Use();

			RefractionShader.SetInteger("u_NormalTexture", 1);
			RefractionShader.SetInteger("u_DepthTexture", 2);
			RefractionShader.SetInteger("u_RefractionMaskTexture", 3);
			RefractionShader.SetInteger("u_NoiseTexture", 4);

			RefractionShader.SetMatrix4("u_ProjectionMatrix", Player.Camera.GetProjectionMatrix());
			RefractionShader.SetMatrix4("u_ViewMatrix", Player.Camera.GetViewMatrix());
			RefractionShader.SetMatrix4("u_InverseProjectionMatrix", glm::inverse(Player.Camera.GetProjectionMatrix()));
			RefractionShader.SetFloat("u_zNear", 0.1f);
			RefractionShader.SetFloat("u_zFar", 1000.0f);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, PreviousFrameFBO.GetNormalTexture());

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, PreviousFrameFBO.GetDepthTexture());

			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, PreviousFrameFBO.GetRefractionMaskTexture());

			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, BlueNoiseTexture.GetTextureID());

			FBOVAO.Bind();
			glDrawArrays(GL_TRIANGLES, 0, 6);
			FBOVAO.Unbind();

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glUseProgram(0);

			AppRenderingTime.Refractions = t2.End();
		}

		else
		{
			Blocks::Timer t2;
			t2.Start();

			RefractionFBO.Bind();
			glViewport(0, 0, RefractionFBO.GetWidth(), RefractionFBO.GetHeight());

			glClearColor(-1.0f, -1.0f, -1.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glUseProgram(0);

			glClearColor(173.0f / 255.0f, 216.0f / 255.0f, 230.0f / 255.0f, 1.0f);
			
			AppRenderingTime.Refractions = t2.End();
		}

		// ---------------------
		// Depth pre pass

		if (DepthPrePass)
		{
			CurrentlyUsedFBO.Bind();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glEnable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			glFrontFace(GL_CW);
			glDepthMask(GL_TRUE);

			DepthPrepassShader.Use();
			glViewport(0, 0, CurrentlyUsedFBO.GetDimensions().first, CurrentlyUsedFBO.GetDimensions().second);

			DepthPrepassShader.SetMatrix4("u_ViewProjection", Player.Camera.GetViewProjection());
			MainWorld.RenderChunks(Player.Camera.GetPosition(), Player.PlayerViewFrustum, DepthPrepassShader);

			glUseProgram(0);
		}

		// Do the normal rendering

		// ---------------------
		// Normal render pass

		CurrentlyUsedFBO.Bind();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// --------------------
		// Render the atmosphere

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDepthMask(GL_FALSE);

		AtmosphereCombineShader.Use();
		AtmosphereCombineShader.SetMatrix4("u_InverseProjection", glm::inverse(Player.Camera.GetProjectionMatrix()));
		AtmosphereCombineShader.SetMatrix4("u_InverseView", glm::inverse(Player.Camera.GetViewMatrix()));
		AtmosphereCombineShader.SetInteger("u_AtmosphereTexture", 3);
		AtmosphereCombineShader.SetVector3f("u_SunDir", SunDirection);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_CUBE_MAP, AtmosphereCubemap.GetTexture());

		FBOVAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		FBOVAO.Unbind();

		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		// ------------------------
		// Render the actual chunks

		Blocks::Timer t3;
		t3.Start();

		glViewport(0, 0, CurrentlyUsedFBO.GetDimensions().first, CurrentlyUsedFBO.GetDimensions().second);

		// Prepare to render the chunks
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CW);

		RenderShader.Use();

		RenderShader.SetMatrix4("u_Model", glm::mat4(1.0f));
		RenderShader.SetMatrix4("u_Projection", Player.Camera.GetProjectionMatrix());
		RenderShader.SetMatrix4("u_View", Player.Camera.GetViewMatrix());
		RenderShader.SetInteger("u_BlockTextures", 0);
		RenderShader.SetInteger("u_BlockNormalTextures", 1);
		RenderShader.SetInteger("u_BlockPBRTextures", 2);
		RenderShader.SetVector3f("u_ViewerPosition", Player.Camera.GetPosition());
		RenderShader.SetFloat("u_ShadowBias", ShadowBias);

		// Shadows
		RenderShader.SetInteger("u_LightShadowMap", 3);
		RenderShader.SetMatrix4("u_LightViewMatrix", Blocks::ShadowMapRenderer::GetLightViewMatrix());
		RenderShader.SetMatrix4("u_LightProjectionMatrix", Blocks::ShadowMapRenderer::GetLightProjectionMatrix());
		
		// Misc
		RenderShader.SetInteger("u_BlueNoiseTexture", 4);
		RenderShader.SetFloat("u_GraniteTexIndex", Blocks::BlockDatabase::GetBlockTexture("polished_granite", Blocks::BlockFaceType::Top));
		RenderShader.SetFloat("u_Time", glfwGetTime());
		RenderShader.SetInteger("u_PreviousFrameColorTexture", 5);
		RenderShader.SetInteger("u_SSRTexture", 6);
		RenderShader.SetInteger("u_ReflectionCubemap", 7);
		RenderShader.SetInteger("u_AtmosphereCubemap", 8);
		RenderShader.SetVector2f("u_Dimensions", glm::vec2(CurrentlyUsedFBO.GetDimensions().first, CurrentlyUsedFBO.GetDimensions().second));
		RenderShader.SetBool("u_SSREnabled", _SSR);

		RenderShader.SetVector3f("u_SunDirection", SunDirection);
		RenderShader.SetVector3f("u_MoonDirection", MoonDirection);

		// Bind Textures

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, Blocks::BlockDatabase::GetTextureArray());

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D_ARRAY, Blocks::BlockDatabase::GetNormalTextureArray());

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D_ARRAY, Blocks::BlockDatabase::GetPBRTextureArray());

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, ShadowMap.GetDepthTexture());

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, BlueNoiseTexture.GetTextureID());

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, PreviousFrameFBO.GetColorTexture());

		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_2D, SSRFBO.GetTexture());

		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_CUBE_MAP, ReflectionMap.GetTexture());

		glActiveTexture(GL_TEXTURE8);
		glBindTexture(GL_TEXTURE_CUBE_MAP, AtmosphereCubemap.GetTexture());

		MainWorld.RenderChunks(Player.Camera.GetPosition(), Player.PlayerViewFrustum, RenderShader);

		AppRenderingTime.Rendering = t3.End();

		glDepthMask(GL_TRUE);

		// ---------------	
		// Blit the fbo to a temporary one for fake refractions and for bloom

		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);

		CurrentlyUsedFBO.Bind();
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, TempFBO.GetFramebuffer());
		glBlitFramebuffer(0, 0, TempFBO.GetWidth(), TempFBO.GetHeight(), 0, 0, TempFBO.GetWidth(), TempFBO.GetHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// ----------------
		// Water rendering

		Blocks::Timer t4;
		t4.Start();

		CurrentlyUsedFBO.Bind();
		WaterShader.Use(); 

		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		if (!ShouldDoRefractions)
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}

		WaterShader.SetMatrix4("u_View", Player.Camera.GetViewMatrix());
		WaterShader.SetMatrix4("u_Projection", Player.Camera.GetProjectionMatrix());
		WaterShader.SetMatrix4("u_InverseView", glm::inverse(Player.Camera.GetViewMatrix()));
		WaterShader.SetMatrix4("u_InverseProjection", glm::inverse(Player.Camera.GetProjectionMatrix()));

		WaterShader.SetInteger("u_PreviousFrameColorTexture", 0);
		WaterShader.SetInteger("u_SSRTexture", 1);
		WaterShader.SetInteger("u_NoiseTexture", 2);
		WaterShader.SetInteger("u_NoiseNormalTexture", 3);
		WaterShader.SetInteger("u_RefractionTexture", 5);
		WaterShader.SetInteger("u_FallbackReflectionTexture", 6);
		WaterShader.SetInteger("u_WaterDetailNormalMap", 7);
		WaterShader.SetInteger("u_WaterMap[0]", 8);
		WaterShader.SetInteger("u_WaterMap[1]", 9);
		WaterShader.SetInteger("u_RefractionUVTexture", 10);
		WaterShader.SetInteger("u_PreviousFrameDepthTexture", 11);
		WaterShader.SetInteger("u_AtmosphereCubemap", 12);
		WaterShader.SetInteger("u_CurrentFrame", app.GetCurrentFrame());

		WaterShader.SetVector2f("u_Dimensions", glm::vec2(CurrentlyUsedFBO.GetDimensions().first, CurrentlyUsedFBO.GetDimensions().second));
		WaterShader.SetBool("u_SSREnabled", _SSR);
		WaterShader.SetBool("u_RefractionsEnabled", ShouldDoRefractions);
		WaterShader.SetBool("u_EnableParallax", ShouldDoWaterParallax);
		WaterShader.SetFloat("u_Time", glfwGetTime());
		WaterShader.SetFloat("u_MixAmount", (float)(app.GetCurrentFrame() % 4) / (float)(32.0f));
		WaterShader.SetFloat("u_ParallaxDepth", WaterParallaxDepth);
		WaterShader.SetFloat("u_ParallaxScale", WaterParallaxHeight);
		WaterShader.SetVector3f("u_SunDirection", SunDirection);
		WaterShader.SetVector3f("u_ViewerPosition", Player.Camera.GetPosition());

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, PreviousFrameFBO.GetColorTexture());

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, SSRFBO.GetTexture());

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, PerlinNoiseTexture.GetTextureID());

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, PerlinNoiseNormalTexture.GetTextureID());

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, TempFBO.GetTexture());

		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_CUBE_MAP, ReflectionMap.GetTexture());

		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_2D, WaterDetailNormalMap.GetTextureID());

		unsigned int idx = (((app.GetCurrentFrame() / 6)) % 100 + 100) % 100;
		unsigned int idx_1 = (((app.GetCurrentFrame() / 6) + 1) % 100 + 100) % 100;

		glActiveTexture(GL_TEXTURE8);
		glBindTexture(GL_TEXTURE_2D, WaterMaps[idx].GetTextureID());

		glActiveTexture(GL_TEXTURE9);
		glBindTexture(GL_TEXTURE_2D, WaterMaps[idx_1].GetTextureID());

		glActiveTexture(GL_TEXTURE10);
		glBindTexture(GL_TEXTURE_2D, RefractionFBO.GetTexture());

		glActiveTexture(GL_TEXTURE11);
		glBindTexture(GL_TEXTURE_2D, PreviousFrameFBO.GetDepthTexture());

		glActiveTexture(GL_TEXTURE12);
		glBindTexture(GL_TEXTURE_CUBE_MAP, AtmosphereCubemap.GetTexture());

		MainWorld.RenderWaterChunks(Player.Camera.GetPosition(), Player.PlayerViewFrustum, WaterShader);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glUseProgram(0);

		AppRenderingTime.Water = t4.End();

		// ----------------
		// Volumetric lighting pass

		if (ShouldRenderVolumetrics)
		{
			Blocks::Timer t5;
			t5.Start();

			VolumetricLightingFBO.Bind();

			glViewport(0, 0, VolumetricLightingFBO.GetWidth(), VolumetricLightingFBO.GetHeight());
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);

			VolumetricShader.Use();

			VolumetricShader.SetInteger("u_DepthTexture", 0);
			VolumetricShader.SetInteger("u_ShadowMap", 1);
			VolumetricShader.SetInteger("u_NoiseTexture", 2);

			VolumetricShader.SetMatrix4("u_InverseProjectionMatrix", glm::inverse(Player.Camera.GetProjectionMatrix()));
			VolumetricShader.SetMatrix4("u_InverseViewMatrix", glm::inverse(Player.Camera.GetViewMatrix()));
			VolumetricShader.SetMatrix4("u_LightViewProjection", Blocks::ShadowMapRenderer::GetLightProjectionMatrix() * Blocks::ShadowMapRenderer::GetLightViewMatrix());
			VolumetricShader.SetVector3f("u_ViewerPosition", Player.Camera.GetPosition());
			VolumetricShader.SetVector3f("u_LightDirection", glm::normalize(LightDirectionToUse));
			VolumetricShader.SetInteger("u_Width", VolumetricLightingFBO.GetWidth());
			VolumetricShader.SetInteger("u_Height", VolumetricLightingFBO.GetHeight());
			VolumetricShader.SetFloat("u_Scattering", VolumetricScattering);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, CurrentlyUsedFBO.GetDepthTexture());

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, ShadowMap.GetDepthTexture());

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, BlueNoiseTexture.GetTextureID());

			FBOVAO.Bind();
			glDrawArrays(GL_TRIANGLES, 0, 6);
			FBOVAO.Unbind();

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glUseProgram(0);

			AppRenderingTime.Volumetrics = t5.End();
		}

		// --------------
		// B L O O M.. Pass

		if (_Bloom)
		{
			Blocks::Timer t6;
			t6.Start();

			Blocks::BloomRenderer::RenderBloom(BloomFBO, CurrentlyUsedFBO.GetColorTexture());

			AppRenderingTime.Bloom = t6.End();
		}

		// --------------
		// After the rendering, do the tonemapping pass

		Blocks::Timer t7;
		t7.Start();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, app.GetWidth(), app.GetHeight());
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		PPShader.Use();
		PPShader.SetInteger("u_FramebufferTexture", 0);
		PPShader.SetInteger("u_VolumetricTexture", 1);
		PPShader.SetInteger("u_AtmosphereTexture", 3);
		PPShader.SetInteger("u_DepthTexture", 4);

		// Bloom textures
		PPShader.SetInteger("u_BloomTextures[0]", 5);
		PPShader.SetInteger("u_BloomTextures[1]", 6);

		PPShader.SetBool("u_BloomEnabled", _Bloom);
		PPShader.SetBool("u_VolumetricEnabled", ShouldRenderVolumetrics);
		PPShader.SetBool("u_PlayerInWater", Player.InWater);
		PPShader.SetFloat("u_Exposure", Exposure);
		PPShader.SetFloat("u_Time", glfwGetTime());
		PPShader.SetMatrix4("u_InverseProjection", glm::inverse(Player.Camera.GetProjectionMatrix()));
		PPShader.SetMatrix4("u_InverseView", glm::inverse(Player.Camera.GetViewMatrix()));
		PPShader.SetVector3f("u_SunDirection", -SunDirection);
		PPShader.SetVector2f("u_Dimensions", glm::vec2(CurrentlyUsedFBO.GetWidth(), CurrentlyUsedFBO.GetHeight()));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, CurrentlyUsedFBO.GetColorTexture());
		
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, VolumetricLightingFBO.GetTexture());

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_CUBE_MAP, AtmosphereCubemap.GetTexture());

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, CurrentlyUsedFBO.GetDepthTexture());

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, BloomFBO.m_Mip0);

		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_2D, BloomFBO.m_Mip1);

		FBOVAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		FBOVAO.Unbind();

		AppRenderingTime.PostProcessing = t7.End();

		// Render the 2D elements
		Renderer2D.RenderQuad(glm::vec2(floor((float)app.GetWidth() / 2.0f), floor((float)app.GetHeight() / 2.0f)), &Crosshair, &OCamera);

		AppRenderingTime.TotalMeasured = update_timer.End();

		app.FinishFrame();
		GLClasses::DisplayFrameRate(app.GetWindow(), "Blocks");
		Player.Camera.Refresh();

		// Write the default values for flags that change from frame to frame
		PlayerMoved = false;
		BlockModified = false;
		SunDirectionChanged = false;
	}
}

// Forward declarations 
namespace Blocks
{
	Block GetWorldBlock(const glm::ivec3& block)
	{
		return MainWorld.GetWorldBlock(block);
	}

	Block* GetWorldBlockPtr(const glm::ivec3& block)
	{
		return MainWorld.GetWorldBlockPtr(block);
	}

	uint8_t GetWorldBlockLight(const glm::ivec3& block)
	{
		return MainWorld.GetWorldBlockLightValue(block);
	}
}



