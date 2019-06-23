solution "invaders-emulator"
	location "../build"
	configurations { "Debug", "Dev", "Release" }
	platforms { "native", "x32", "x64" }

	project "disassemble"
		location "../build"
		kind "ConsoleApp"
		language "C++"
		files {
			"../src/disassemble.cpp",
			"../src/8080.*",
			"../src/Assert.*",
			"../src/Helpers.*"
		}
		includedirs {
			"../src/**"
		}
		flags { "ExtraWarnings", "FatalWarnings" }
		debugdir "../data"		-- debugger working directory
		
		configuration "Debug"
			--defines { "_DEBUG" } Removed this Windows define because using /MD rather than /MDd
			defines { "DEBUG" }
			flags { "Symbols" }
			targetdir "../bin/debug"

		configuration "Dev"
			flags { "Optimize", "Symbols" }			
			targetdir "../bin/dev"
	
		configuration "Release"
			defines { "NDEBUG", "RELEASE" }
			flags { "Optimize" }
			targetdir "../bin/release"
			
		configuration "windows"
			flags { "ReleaseRuntime" }  
			defines { "_CRT_SECURE_NO_WARNINGS" }
			
			-- Disable compiler warnings. These end up in the Project Settings -> C/C++ -> Command Line -> Additional Options, rather than C/C++ -> Advanced -> Disable Specific Warnings 
			buildoptions { "/wd4127" } -- conditional expression is constant
			buildoptions { "/wd4505" } -- unreferenced local function has been removed
			
		configuration "linux"
			buildoptions { "-std=c++0x" }
			buildoptions { "-Wno-switch" }
			buildoptions { "-Wno-unused-function" }
			buildoptions { "-Wno-missing-field-initializers" }
			buildoptions { "-Wno-missing-braces" }

	project "emulator"
		location "../build"
		kind "ConsoleApp"
		language "C++"
		files {
			"../README.md",
			"../src/invaders.cpp",
			"../src/8080.*",
			"../src/machine.*",
			"../src/debugger.*",
			"../src/Assert.*",
			"../src/Helpers.*",
			"../src/imgui_memory_editor.h",
			"../src/imgui/**",
			"../3rdParty/gl3w/GL/**"
		}
		includedirs {
			"../src/**",
			"../3rdparty/SDL2-2.0.9/include",
			"../3rdparty/gl3w"
		}
		flags { "ExtraWarnings", "FatalWarnings" }
		links { "SDL2", "opengl32" }
		debugdir "../data"		-- debugger working directory
		
		configuration "Debug"
			--defines { "_DEBUG" } Removed this Windows define because using /MD rather than /MDd
			defines { "DEBUG" }
			flags { "Symbols" }
			targetdir "../bin/debug"

		configuration "Dev"
			flags { "Optimize", "Symbols" }			
			targetdir "../bin/dev"
	
		configuration "Release"
			defines { "NDEBUG", "RELEASE" }
			flags { "Optimize" }
			targetdir "../bin/release"
			
		configuration "windows"
			flags { "ReleaseRuntime" }  
			links { "SDL2main" }
			defines { "_CRT_SECURE_NO_WARNINGS" }
			
			-- Disable compiler warnings. These end up in the Project Settings -> C/C++ -> Command Line -> Additional Options, rather than C/C++ -> Advanced -> Disable Specific Warnings 
			buildoptions { "/wd4127" } -- conditional expression is constant
			buildoptions { "/wd4505" } -- unreferenced local function has been removed

		configuration { "windows", "not x64" }
			libdirs { 
				"../3rdparty/SDL2-2.0.9/lib/x86"
			}
			postbuildcommands { 
				"copy ..\\3rdparty\\SDL2-2.0.9\\lib\\x86\\*.dll ..\\bin\\$(ConfigurationName)"
			}

		configuration { "windows", "x64" }		
			libdirs { 
				"../3rdparty/SDL2-2.0.9/lib/x64"
			}
			postbuildcommands { 
				"copy ..\\3rdparty\\SDL2-2.0.9\\lib\\x64\\*.dll ..\\bin\\$(ConfigurationName)"
			}
			
		configuration "linux"
			buildoptions { "-std=c++0x" }
			buildoptions { "-Wno-switch" }
			buildoptions { "-Wno-unused-function" }
			buildoptions { "-Wno-missing-field-initializers" }
			buildoptions { "-Wno-missing-braces" }
								
newaction
{
	trigger = "clean",
	shortname = "clean",
	description = "Removes generated files."
}
			
if _ACTION == "clean" then
	os.rmdir("../bin")
	if os.get() == "windows" then
		os.outputof("rmdir ..\\build\\.vs /s /q") -- remove the hidden .vs directory
	end
	os.rmdir("../build")			-- this doesn't work because the directory contains .vs folder
end
