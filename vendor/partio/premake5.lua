project "partio"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"

	includedirs {
		"src",
		"%{wks.location}/vendor/zlib/src"
	}

	files "src/**"

	links "zlib"

	filter "platforms:Win64"
		defines { "PARTIO_WIN32", "PARTIO_USE_ZLIB" }
		characterset "MBCS"
