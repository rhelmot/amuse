function(bintoc out in sym)
  if(IS_ABSOLUTE ${out})
    set(theOut ${out})
  else()
    set(theOut ${CMAKE_CURRENT_BINARY_DIR}/${out})
  endif()
  if(IS_ABSOLUTE ${in})
    set(theIn ${in})
  else()
    set(theIn ${CMAKE_CURRENT_SOURCE_DIR}/${in})
  endif()
  get_filename_component(outDir ${theOut} DIRECTORY)
  file(MAKE_DIRECTORY ${outDir})
  add_custom_command(OUTPUT ${theOut}
                     COMMAND "bintoc" ARGS ${theIn} ${theOut} ${sym}
                     DEPENDS ${theIn})
endfunction()

function(bintoc_compress out in sym)
  if(IS_ABSOLUTE ${out})
    set(theOut ${out})
  else()
    set(theOut ${CMAKE_CURRENT_BINARY_DIR}/${out})
  endif()
  if(IS_ABSOLUTE ${in})
    set(theIn ${in})
  else()
    set(theIn ${CMAKE_CURRENT_SOURCE_DIR}/${in})
  endif()
  get_filename_component(outDir ${theOut} DIRECTORY)
  file(MAKE_DIRECTORY ${outDir})
  add_custom_command(OUTPUT ${theOut}
                     COMMAND "bintoc" ARGS --compress ${theIn} ${theOut} ${sym}
                     DEPENDS ${theIn})
endfunction()
