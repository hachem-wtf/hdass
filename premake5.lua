output_dir = "%{cfg.buildcfg}-%{cfg.system}"

function setup_target()
	targetdir ("bin/" .. output_dir)
	objdir ("bin-int/" .. output_dir .. "/%{prj.name}")
	staticruntime "On"
end

function setup_c_target()
	setup_target()

	filter "configurations:debug"
		runtime "Debug"
		symbols "On"
	filter { "configurations:release", "configurations:dist" }
		runtime "Release"
		optimize "Speed"
	filter "configurations:dist"
		symbols "Off"
	filter {}
end

newaction {
	trigger = "clean",
	description = "Remove build output and generated project files",

	execute = function()
		os.rmdir("bin")
		os.rmdir("bin-int")
		os.remove("Makefile")

		for _, file in ipairs(os.matchfiles("*.make")) do
			os.remove(file)
		end

		print("Cleaned build output and generated project files")
	end
}

workspace "hdass"
	architecture "x64"
	startproject "hdass"
	multiprocessorcompile "On"

	configurations {
		"debug",
		"release",
		"dist",
	}

	filter "system:windows"
		defines "HDASS_WINDOWS"
	filter "system:linux"
		defines "HDASS_LINUX"
	filter "system:macosx"
		architecture "ARM64"
		defines "HDASS_MACOS"
	filter {}

project "hdass"
	kind "ConsoleApp"
	language "C"
	cdialect "C17"

	setup_c_target()

	files {
		"src/**.h",
		"src/**.c",
	}

	includedirs "src"

	filter { "system:macosx or system:linux" }
		buildoptions {
			"-Wall",
			"-Wextra",
			"-Werror",
			"-pedantic",
		}
	filter {}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:debug"
		defines "HDASS_DEBUG"
	filter "configurations:release"
		defines "HDASS_RELEASE"
	filter "configurations:dist"
		defines "HDASS_DIST"
	filter {}
