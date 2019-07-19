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
			"../src"
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
			buildoptions { "-Wno-switch" }
			buildoptions { "-Wno-unused-function" }
			buildoptions { "-Wno-missing-braces" }

		configuration "gcc"
			buildoptions_cpp { "-std=c++11" }
			buildoptions { "-Wno-missing-field-initializers" }
			
	project "emulator"
		location "../build"
		kind "ConsoleApp"
		language "C++"
		files {
			"../README.md",
			"../src/invaders.cpp",
			"../src/8080.*",
			"../src/machine.*",
			"../src/Assert.*",
			"../src/Helpers.*",
			"../src/Audio.*",
			"../src/Input.*",
			"../src/imgui/**",
			"../src/debugger/*",
			"../3rdParty/gl3w/GL/**"
		}
		includedirs {
			"../src",
			"../3rdParty/gl3w"
		}
		flags { "ExtraWarnings", "FatalWarnings" }
		links { "SDL2_mixer" }  -- not in the string returned by `sdl2-config --links`
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
			includedirs {
				"../3rdParty/SDL2-2.0.9/include",
				"../3rdParty/SDL2_mixer-2.0.4/include"
			}
			flags { "ReleaseRuntime" }  
			links { "SDL2", "SDL2main", "opengl32" }
			defines { "_CRT_SECURE_NO_WARNINGS" }
			
			-- Disable compiler warnings. These end up in the Project Settings -> C/C++ -> Command Line -> Additional Options, rather than C/C++ -> Advanced -> Disable Specific Warnings 
			buildoptions { 
				"/wd4127", -- conditional expression is constant
				"/wd4505" -- unreferenced local function has been removed
			}

		configuration { "windows", "release" }
			buildoptions "/wd4390" -- empty controlled statement found; is this the intent? Required for ImGui in release

		configuration { "windows", "not x64" }
			libdirs { 
				"../3rdParty/SDL2-2.0.9/lib/x86",
				"../3rdParty/SDL2_mixer-2.0.4/lib/x86"
			}
			postbuildcommands { 
				"copy ..\\3rdParty\\SDL2-2.0.9\\lib\\x86\\*.dll ..\\bin\\$(ConfigurationName)",
				"copy ..\\3rdParty\\SDL2_mixer-2.0.4\\lib\\x86\\*.dll ..\\bin\\$(ConfigurationName)"
			}

		configuration { "windows", "x64" }		
			libdirs { 
				"../3rdParty/SDL2-2.0.9/lib/x64",
				"../3rdParty/SDL2_mixer-2.0.4/lib/x64"
			}
			postbuildcommands { 
				"copy ..\\3rdParty\\SDL2-2.0.9\\lib\\x64\\*.dll ..\\bin\\$(ConfigurationName)",
				"copy ..\\3rdParty\\SDL2_mixer-2.0.4\\lib\\x64\\*.dll ..\\bin\\$(ConfigurationName)"
			}
			
		configuration "linux"
			buildoptions_cpp { "-std=c++11" }
			buildoptions { "-Wno-switch" }
			buildoptions { "-Wno-unused-function" }
			buildoptions { "-Wno-missing-field-initializers" }
			buildoptions { "-Wno-missing-braces" }
			
			-- ImGui with my HP_ASSERT macro gives: error: assuming signed overflow does not occur when assuming that (X - c) > X is always false [-Werror=strict-overflow]
			buildoptions { "-Wno-strict-overflow" }
			
			buildoptions { "`sdl2-config --cflags`" }  -- magic quotes are shell-dependent
			linkoptions { "`sdl2-config --libs`" } -- magic quotes are shell-dependent

			--libdirs { "/opt/vc/lib" } -- really just Raspberry Pi only (VideoCore) 
			links { "EGL", "GLESv2", "GL", "dl" }
			
		configuration { "linux", "release" }
			buildoptions { "-Wno-empty-body" } -- ImGui GCC release error: suggest braces around empty body in an ‘if’ statement [-Werror=empty-body] (assert related)

		configuration "macosx"
			buildoptions { "-std=c++11" }
			buildoptions { "-Wno-unused-function" }
			buildoptions { "-Wno-missing-braces" }

		configuration { "macosx", "xcode*" }
if os.get() == "macosx" then
			buildoptions { os.outputof("sdl2-config --cflags") } -- magic quotes are no good for Xcode so can't use `sdl2-config --cflags`
			linkoptions { os.outputof("sdl2-config --libs") }
end

		configuration { "macosx", "not xcode*" }
			buildoptions { "`sdl2-config --cflags`" } -- magic quotes are no good for Xcode
			linkoptions { "`sdl2-config --libs`" }

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
	if os.get() == "macosx" then
		os.outputof("rm -rf build") -- remove the build folder, including hidden .DS_Store file
	end
	os.rmdir("../build")			-- this doesn't work because the directory contains .vs folder
end
