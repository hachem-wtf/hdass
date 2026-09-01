output_dir = "%{cfg.buildcfg}-%{cfg.system}"

function setup_target()
	targetdir ("bin/" .. output_dir)
	objdir ("bin-int/" .. output_dir .. "/%{prj.name}")
	staticruntime "On"
end

function setup_c_target()
	setup_target()

	filter { "system:macosx or system:linux" }
		buildoptions {
			"-Wall",
			"-Wextra",
			"-Wpedantic",
			"-Wconversion",
			"-Wsign-conversion",
			"-Wshadow",
			"-Wstrict-prototypes",
			"-Wmissing-prototypes",
		}
	filter {}

	filter "configurations:debug"
		runtime "Debug"
		symbols "On"

		filter { "system:macosx or system:linux" }
			buildoptions {
				"-g",
				"-fno-omit-frame-pointer",
				"-fsanitize=address,undefined",
			}

			linkoptions {
				"-fsanitize=address,undefined",
			}
		filter {}

	filter { "configurations:release", "configurations:dist" }
		runtime "Release"
		optimize "Speed"
	filter {}

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

newaction {
	trigger = "check",
	description = "Run static analysis with cppcheck",

	execute = function()
		local result = os.execute("make clean")

		if result ~= true and result ~= 0 then
			error("Failed to clean project")
		end

		result = os.execute(
			"bear -- make config=debug"
		)

		if result ~= true and result ~= 0 then
			error("Failed to generate compile_commands.json")
		end

		result = os.execute(
			"cppcheck " ..
			"--project=compile_commands.json " ..
			"--file-filter=src/** " ..
			"--file-filter=tests/** " ..
			"--enable=warning,style,performance,portability " ..
			"--error-exitcode=1"
		)

		if result ~= true and result ~= 0 then
			error("cppcheck found issues")
		end
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

	filter "system:windows"
		systemversion "latest"

	filter "configurations:debug"
		defines "HDASS_DEBUG"

	filter "configurations:release"
		defines "HDASS_RELEASE"

	filter "configurations:dist"
		defines "HDASS_DIST"

	filter {}

project "tests"
	kind "ConsoleApp"
	language "C"
	cdialect "C17"

	setup_c_target()

	files {
		"src/**.h",
		"src/**.c",
		"tests/**.h",
		"tests/**.c",
	}

	removefiles "src/main.c"

	includedirs {
		"src",
		"tests",
	}

	filter "system:windows"
		systemversion "latest"

	filter {}

