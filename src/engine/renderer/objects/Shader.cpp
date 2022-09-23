#include "engine/hzpch.h"
#include "engine/renderer/objects/Shader.h"

#include "engine/assets/AssetManager.h"
#include "engine/renderer/Renderer.h"

#include <fstream>

#if !PRODUCTION
	#include <shaderc/shaderc.hpp>
#endif

std::string ReadTextFile(const AssetPathAbs& filename)
{
	std::ifstream file(filename.string(), std::ios::ate | std::ios::binary);
	std::vector<char> buffer(size_t(file.tellg()));

	file.seekg(0);
	file.read(buffer.data(), buffer.size());
	file.close();

	return std::string(buffer.data(), buffer.size());
}

void ReadBinaryFile(const AssetPathAbs& filename, std::vector<uint32_t>& buffer)
{
	std::ifstream file(filename.string(), std::ios::ate | std::ios::binary);
	size_t bytes = size_t(file.tellg());
	buffer.resize(bytes / sizeof(uint32_t));

	file.seekg(0);
	file.read((char*)buffer.data(), bytes);
	file.close();
}

size_t ReadHash(const AssetPathAbs& filename)
{
	std::ifstream file(filename.string(), std::ios::binary);

	if (file.is_open())
	{
		size_t hash{};
		file.read((char*)&hash, sizeof(hash));
		return hash;
	}

	return 0;
}

bool Shader::Create(
	const AssetPathRel& path,
	vk::ShaderStageFlagBits stage,
	const std::vector<DescriptorSetLayoutBinding>& descriptorSetLayoutBindings)
{
	bool isEngineFile = path.IsEngineFile();

	// get file paths
	AssetPathRel spvFilepath = path.ApplyFixes("compiled_", ".spv");
	AssetPathAbs absSpvFilepath = spvFilepath.MakeAbsolute(isEngineFile);

#if !PRODUCTION
	AssetPathRel hashFilepath = (path + ".hash").MakeTemp();
	AssetPathAbs absHashFilepath = hashFilepath.MakeAbsolute(isEngineFile);

	// assert file exists
	auto absPath = path.MakeAbsolute(isEngineFile);

	if (!absPath.Exists())
	{
		SPDLOG_ERROR("Shader file '{}' does not exist!", path);
		return false;
	}

	// read precomputed hash and compute new hash
	const auto storedHash = ReadHash(absHashFilepath);
	const auto codeText = ReadTextFile(absPath);
	const auto computedHash = std::hash<std::string>()(codeText);

	// recompile if necessary
	if (storedHash != computedHash)
	{
		if (storedHash != 0)
			SPDLOG_INFO("Shader code changed. Recompiling '{}'...", path);

#if 0
		shaderc::Compiler c;

		std::string filename = path.operator const std::filesystem::path &().string();
		auto result = c.CompileGlslToSpv(codeText, shaderc_glsl_vertex_shader, filename.c_str());

		if (!result.cbegin())
		{
			SPDLOG_ERROR("Failed to compile shader '{}'! Here is the error:", path);
			SPDLOG_ERROR(result.GetErrorMessage());

			return false;
		}

		std::ofstream file;
		file.open(absPath, std::ios::trunc | std::ios::binary);
		file.write(reinterpret_cast<const char*>(result.cbegin()), result.cend() - result.cbegin());
		file.close();
#endif

		// call glslc
		std::string cmd = "E:\\dev\\VulkanSDK\\1.3.224.1\\Bin\\glslc " + absPath.string() + " -o " + absSpvFilepath.string();
		if (system(cmd.c_str()))
		{
			SPDLOG_ERROR("Failed to compile shader '{}'!", path);
			return false;
		}

		// store hash
		std::filesystem::create_directories(absHashFilepath.path().parent_path());

		std::ofstream file(absHashFilepath.string(), std::ios::binary);
		HZ_ASSERT(file.is_open(), "Failed to open shader hash file! ({})", path);

		file.write((char*)&computedHash, sizeof(computedHash));
		file.close();
	}
#endif

	// load compiled code
	std::vector<uint32_t> code;
	ReadBinaryFile(absSpvFilepath, code);

	// create module
	vk::ShaderModuleCreateInfo info({}, code.size() * sizeof(uint32_t), code.data());
	Module = Renderer::GetInstance().Device.createShaderModule(info);

	if (!Module)
	{
		SPDLOG_ERROR("Failed to load shader '{}'!", path);
		return false;
	}

	Stage = stage;

	ShaderStageCreateInfo
		.setStage(Stage)
		.setModule(Module)
		.setPName("main");

	for (auto& binding : descriptorSetLayoutBindings)
		DescriptorSetLayoutBindings.push_back(
			vk::DescriptorSetLayoutBinding{
				binding.Binding,
				binding.Type,
				binding.Count,
				binding.StageFlags ? binding.StageFlags : Stage,
			}
		);

	SPDLOG_INFO("Loaded shader '{}'!", path);

	return operator bool();
}

void Shader::Destroy()
{
	if (Module)
	{
		Renderer::GetInstance().Device.destroyShaderModule(Module);
		Module = nullptr;
	}
}
