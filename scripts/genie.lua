solution "invaders-emulator"
	location "../build"
	configurations { "Debug", "Dev", "Release" }
	platforms { "native", "x32", "x64" }

	configuration "windows"		
					
		buildoptions { 
			-- Disable compiler warnings. These end up in the Project Settings -> C/C++ -> Command Line -> Additional Options, rather than C/C++ -> Advanced -> Disable Specific Warnings 
			"/wd4127", -- conditional expression is constant
			"/wd4505", -- unreferenced local function has been removed	
			
			-- Treat warnings as errors
			-- TODO: Test gcc/clang equivalent
			--"/we4061", -- enumerator 'identifier' in switch of enum 'enumeration' is not explicitly handled by a case label. i.e. Warn even if there is a default
			"/we4062"  -- enumerator 'identifier' in switch of enum 'enumeration' is not handled
		}
		
	configuration "gcc"	
		buildoptions { 
			"-Wswitch"        -- warn when switch is missing case
			--"-Wswitch-enum" -- warn when switch is missing case, even if there is a default
		}

	configuration "x64"
		targetdir "../bin/x64"

	configuration "not x64"
		targetdir "../bin/x86"

	configuration "Debug"
		--defines { "_DEBUG" } Removed this Windows define because using /MD rather than /MDd
		defines { "DEBUG" }
		flags { "Symbols" }
		targetsubdir "debug"

	configuration "Dev"
		flags { "Optimize", "Symbols" }
		targetsubdir "dev"		
	
	configuration "Release"
		defines { "NDEBUG", "RELEASE" }
		flags { "Optimize" }
		targetsubdir "release"	
		
	project "disassemble"
		location "../build"
		kind "ConsoleApp"
		language "C++"
		files {
			"../src/disassemble.cpp",
			"../src/8080.*",
			"../src/hp_assert.*",
			"../src/Helpers.*"
		}
		includedirs {
			"../src"
		}
		flags { "ExtraWarnings", "FatalWarnings" }
		debugdir "../data"		-- debugger working directory
		
		configuration "windows"
			flags { "ReleaseRuntime" }  
			defines { "_CRT_SECURE_NO_WARNINGS" }
			
		configuration "linux"
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
			"../src/hp_assert.*",
			"../src/Helpers.*",
			"../src/Audio.*",
			"../src/Input.*",
			"../src/Display.*",
			"../src/Renderer.*",
			"../src/imgui/**",
			"../src/debugger/*",
			"../libs/gl3w/GL/**"
		}
		includedirs {
			"../src",
			"../libs/gl3w"
		}
		flags { "ExtraWarnings", "FatalWarnings" }
		links { "SDL2_mixer" }  -- not in the string returned by `sdl2-config --links`
		debugdir "../data"		-- debugger working directory
					
		configuration "windows"
			includedirs {
				"../libs/SDL2/include",
				"../libs/SDL2_mixer/include"
			}
			flags { "ReleaseRuntime" }  
			links { "SDL2", "SDL2main", "opengl32" }
			defines { "_CRT_SECURE_NO_WARNINGS" }

		configuration { "windows", "release" }
			buildoptions "/wd4390" -- empty controlled statement found; is this the intent? Required for ImGui in release

		configuration { "windows", "not x64" }
			libdirs { 
				"../libs/SDL2/lib/x86",
				"../libs/SDL2_mixer/lib/x86"
			}
			postbuildcommands { 
				"copy ..\\libs\\SDL2\\lib\\x86\\*.dll ..\\bin\\$(PlatformTarget)\\$(ConfigurationName)",
				"copy ..\\libs\\SDL2_mixer\\lib\\x86\\*.dll ..\\bin\\$(PlatformTarget)\\$(ConfigurationName)"
			}

		configuration { "windows", "x64" }		
			libdirs { 
				"../libs/SDL2/lib/x64",
				"../libs/SDL2_mixer/lib/x64"
			}
			postbuildcommands { 
				"copy ..\\libs\\SDL2\\lib\\x64\\*.dll ..\\bin\\$(PlatformTarget)\\$(ConfigurationName)",
				"copy ..\\libs\\SDL2_mixer\\lib\\x64\\*.dll ..\\bin\\$(PlatformTarget)\\$(ConfigurationName)"
			}

		configuration "gcc"
			buildoptions_cpp { "-std=c++11" }
			buildoptions { "-Wno-missing-field-initializers" }
			
		configuration "linux"
			buildoptions { "-Wno-unused-function" }
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
			buildoptions { "-Wno-unused-function" }
			buildoptions { "-Wno-missing-braces" }

			-- ImGui OSX dependencies from example makefile
			-- OpenGL, Cocoa, IOKit CoreVideo
			linkoptions {
				    "-framework CoreFoundation",
				    "-framework Cocoa"
				    
			}

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
		if os.isdir("../build/.vs") then
			os.outputof("rmdir ..\\build\\.vs /s /q") -- remove the hidden .vs directory
		end
		-- windows sometimes fails to delete the bin folder...
		if os.isdir("../bin") then
			os.outputof("rmdir ..\\bin /s /q")
		end
	end
	if os.get() == "macosx" then
		os.outputof("rm -rf build") -- remove the build folder, including hidden .DS_Store file
	end
	os.rmdir("../build") -- this doesn't work unless the hidden .vs has been removed from the folder first folder
end
