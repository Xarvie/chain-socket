function(PickSourceFiles current_dir variable)
  list(FIND ARGN "${current_dir}" IS_EXCLUDED)
  if(IS_EXCLUDED EQUAL -1)
    file(GLOB COLLECTED_SOURCES
      ${current_dir}/*.c
      ${current_dir}/*.cc
      ${current_dir}/*.cpp
      ${current_dir}/*.cxx
      ${current_dir}/*.h
      ${current_dir}/*.hh
      ${current_dir}/*.hpp)
    list(APPEND ${variable} ${COLLECTED_SOURCES})
    
    file(GLOB SUB_DIRECTORIES ${current_dir}/*)
    foreach(SUB_DIRECTORY ${SUB_DIRECTORIES})
      if (IS_DIRECTORY ${SUB_DIRECTORY})
        PickSourceFiles("${SUB_DIRECTORY}" "${variable}" "${ARGN}")
      endif()
    endforeach()
    set(${variable} ${${variable}} PARENT_SCOPE)
  endif()
endfunction()

function(PickIncludeDirectories current_dir variable)
  list(FIND ARGN "${current_dir}" IS_EXCLUDED)
  if(IS_EXCLUDED EQUAL -1)
    list(APPEND ${variable} ${current_dir})
    file(GLOB SUB_DIRECTORIES ${current_dir}/*)
    foreach(SUB_DIRECTORY ${SUB_DIRECTORIES})
      if (IS_DIRECTORY ${SUB_DIRECTORY})
        PickIncludeDirectories("${SUB_DIRECTORY}" "${variable}" "${ARGN}")
      endif()
    endforeach()
    set(${variable} ${${variable}} PARENT_SCOPE)
  endif()
endfunction()

macro(MakeFilter dir)
file(GLOB_RECURSE elements RELATIVE ${dir} *.h *.hh *.hpp *.c *.cc *.cxx *.cpp)
foreach(element ${elements})
  get_filename_component(element_name ${element} NAME)
  get_filename_component(element_dir ${element} DIRECTORY)
  if (NOT ${element_dir} STREQUAL "")
      string(REPLACE "/" "\\" group_name ${element_dir})
      source_group("${group_name}" FILES ${dir}/${element})
      message("${group_name}" FILES ${dir}/${element})
  else()
    source_group("\\" FILES ${dir}/${element})
    message("\\" FILES ${dir}/${element})
  endif()
endforeach()
endmacro()

macro(configure_files srcDir destDir)
    message(STATUS "Configuring directory ${destDir}")
    make_directory(${destDir})

    file(GLOB templateFiles RELATIVE ${srcDir} ${srcDir}/*)
    foreach(templateFile ${templateFiles})
        set(srcTemplatePath ${srcDir}/${templateFile})
        if(NOT IS_DIRECTORY ${srcTemplatePath})
            message(STATUS "Configuring file ${templateFile}")
            configure_file(
                    ${srcTemplatePath}
                    ${destDir}/${templateFile}
                    @ONLY)
        endif(NOT IS_DIRECTORY ${srcTemplatePath})
    endforeach(templateFile)
endmacro(configure_files)