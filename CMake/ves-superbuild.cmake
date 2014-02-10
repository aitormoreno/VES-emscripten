if(NOT VES_LINUX_SUPERBUILD AND NOT VES_ANDROID_SUPERBUILD AND NOT VES_IOS_SUPERBUILD AND NOT VES_EMSCRIPTEN_SUPERBUILD)
  return()
endif()

include(ExternalProject)

set(base "${CMAKE_BINARY_DIR}/CMakeExternals")
set_property(DIRECTORY PROPERTY EP_BASE ${base})

set(build_type ${CMAKE_BUILD_TYPE})

set(source_prefix ${base}/Source)
set(build_prefix ${base}/Build)
set(install_prefix ${base}/Install)

set(toolchain_dir "${CMAKE_CURRENT_SOURCE_DIR}/CMake/toolchains")
set(ves_src "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(PythonInterp REQUIRED)

set(module_defaults
  -DModule_vtkFiltersCore:BOOL=ON
  -DModule_vtkFiltersSources:BOOL=ON
  -DModule_vtkFiltersGeometry:BOOL=ON
  -DModule_vtkIOGeometry:BOOL=ON
  -DModule_vtkIOXML:BOOL=ON
  -DModule_vtkIOImage:BOOL=ON
  -DModule_vtkIOPLY:BOOL=ON
  -DModule_vtkImagingCore:BOOL=ON
)

macro(force_build proj)
  ExternalProject_Add_Step(${proj} forcebuild
    COMMAND ${CMAKE_COMMAND} -E remove ${base}/Stamp/${proj}/${proj}-build
    DEPENDEES configure
    DEPENDERS build
    ALWAYS 1
  )
endmacro()

macro(install_eigen)
  set(eigen_url http://www.vtk.org/files/support/eigen-3.1.0-alpha1.tar.gz)
  set(eigen_md5 c04dedf4ae97b055b6dd2aaa01daf5e9)
  ExternalProject_Add(
    eigen
    SOURCE_DIR ${source_prefix}/eigen
    URL ${eigen_url}
    URL_MD5 ${eigen_md5}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory "${source_prefix}/eigen/Eigen" "${install_prefix}/eigen/Eigen"
  )
endmacro()

macro(compile_vtk proj)
  ExternalProject_Add(
    ${proj}
    SOURCE_DIR ${source_prefix}/vtkmodular
    GIT_REPOSITORY git://gitorious.org/~patmarion/kitware/patmarion-vtkmodular.git
    GIT_TAG origin/kiwi-new
    INSTALL_COMMAND ""
    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX:PATH=${install_prefix}/${proj}
      -DCMAKE_BUILD_TYPE:STRING=${build_type}
      -DBUILD_SHARED_LIBS:BOOL=ON
      ${module_defaults}
  )
endmacro()

macro(compile_ves proj)
  ExternalProject_Add(
    ${proj}
    SOURCE_DIR ${ves_src}
    DOWNLOAD_COMMAND ""
    DEPENDS vtkmodular-host eigen
    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX:PATH=${install_prefix}/${proj}
      -DCMAKE_BUILD_TYPE:STRING=${build_type}
      -DVES_USE_VTK:BOOL=ON
      -DVTK_DIR:PATH=${build_prefix}/vtkmodular-host
      -DEIGEN_INCLUDE_DIR:PATH=${install_prefix}/eigen
      -DBUILD_TESTING:BOOL=ON
      -DCMAKE_CXX_FLAGS:STRING=${VES_CXX_FLAGS}
  )

  force_build(${proj})
endmacro()

macro(crosscompile_vtk proj toolchain_file)
  ExternalProject_Add(
    ${proj}
    SOURCE_DIR ${base}/Source/vtkmodular
    DOWNLOAD_COMMAND ""
    INSTALL_COMMAND ""
    DEPENDS vtkmodular-host
    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX:PATH=${install_prefix}/${proj}
      -DCMAKE_BUILD_TYPE:STRING=${build_type}
      -DBUILD_SHARED_LIBS:BOOL=OFF
      -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${toolchain_dir}/${toolchain_file}
      -DVTKCompileTools_DIR:PATH=${build_prefix}/vtkmodular-host
      ${module_defaults}
      -C ${toolchain_dir}/TryRunResults.cmake
  )
endmacro()

macro(download_curl)
  ExternalProject_Add(
    curl-download
    GIT_REPOSITORY git://github.com/patmarion/curl.git
    GIT_TAG origin/v7.24.0-with-cmake-patches
    SOURCE_DIR ${source_prefix}/curl
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
  )
endmacro()

macro(crosscompile_curl proj toolchain_file)
  ExternalProject_Add(
    ${proj}
    DOWNLOAD_COMMAND ""
    SOURCE_DIR ${source_prefix}/curl
    DEPENDS curl-download
    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX:PATH=${install_prefix}/${proj}
      -DCMAKE_BUILD_TYPE:STRING=${build_type}
      -DCURL_STATICLIB:BOOL=ON
      -DBUILD_CURL_EXE:BOOL=OFF
      -DBUILD_CURL_TESTS:BOOL=OFF
      -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${toolchain_dir}/${toolchain_file}
      -C ${toolchain_dir}/curl-TryRunResults.cmake
  )
endmacro()

macro(crosscompile_ves proj tag toolchain_file)
  ExternalProject_Add(
    ${proj}
    SOURCE_DIR ${ves_src}
    DOWNLOAD_COMMAND ""
    DEPENDS vtkmodular-${tag} eigen curl-${tag}
    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX:PATH=${install_prefix}/${proj}
      -DCMAKE_BUILD_TYPE:STRING=${build_type}
      -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${toolchain_dir}/${toolchain_file}
      -DCMAKE_CXX_FLAGS:STRING=${VES_CXX_FLAGS}
      -DVES_USE_VTK:BOOL=ON
      -DVTK_DIR:PATH=${build_prefix}/vtkmodular-${tag}
      -DCURL_INCLUDE_DIR:PATH=${install_prefix}/curl-${tag}/include
      -DCURL_LIBRARY:PATH=${install_prefix}/curl-${tag}/lib/libcurl.a
      -DEIGEN_INCLUDE_DIR:PATH=${install_prefix}/eigen
      -DPYTHON_EXECUTABLE:FILEPATH=${PYTHON_EXECUTABLE}
  )

  force_build(${proj})
endmacro()


install_eigen()
compile_vtk(vtkmodular-host)
download_curl()


if(VES_IOS_SUPERBUILD)
  foreach(target ios-simulator ios-device)
    crosscompile_vtk(vtkmodular-${target} toolchain-${target}.cmake)
    crosscompile_curl(curl-${target} toolchain-${target}.cmake)
    crosscompile_ves(ves-${target} ${target} toolchain-${target}.cmake)
  endforeach()
endif()

if(VES_ANDROID_SUPERBUILD)
  crosscompile_vtk(vtkmodular-android android.toolchain.cmake)
  crosscompile_curl(curl-android android.toolchain.cmake)
  crosscompile_ves(ves-android android android.toolchain.cmake)
endif()

if(VES_EMSCRIPTEN_SUPERBUILD)
	crosscompile_vtk(vtkmodular-emscripten emscripten.toolchain.cmake)
	crosscompile_curl(curl-emscripten emscripten.toolchain.cmake)
	crosscompile_ves(ves-emscripten emscripten emscripten.toolchain.cmake)
endif()

if(VES_LINUX_SUPERBUILD)
  compile_ves(ves-host)
endif()

set(ves_superbuild_enabled ON)


# CTestCustom.cmake needs to be placed at the top level build directory
configure_file(${CMAKE_SOURCE_DIR}/CMake/CTestCustom.cmake.in
               ${CMAKE_BINARY_DIR}/CTestCustom.cmake COPYONLY)
