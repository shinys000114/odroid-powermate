# Define the web application source directory and the final output file
set(WEB_APP_SOURCE_DIR ${CMAKE_SOURCE_DIR}/page)
set(GZ_OUTPUT_FILE ${WEB_APP_SOURCE_DIR}/dist/index.html.gz)

# Register the component. Now, CMake knows how GZ_OUTPUT_FILE is generated
# and can correctly handle the dependency for embedding.
idf_component_register(SRC_DIRS "app" "nconfig" "wifi" "indicator" "system" "service" "ina226"
        INCLUDE_DIRS "include"
        EMBED_FILES ${GZ_OUTPUT_FILE}
)

# Define a custom command to build the web app.
# This command explicitly tells CMake that it produces the GZ_OUTPUT_FILE.
add_custom_command(
    OUTPUT ${GZ_OUTPUT_FILE}
    COMMAND npm install
    COMMAND npm run build
    WORKING_DIRECTORY ${WEB_APP_SOURCE_DIR}
    # Re-run the build if package.json or vite.config.js changes
    DEPENDS ${WEB_APP_SOURCE_DIR}/index.html ${WEB_APP_SOURCE_DIR}/src/main.js ${WEB_APP_SOURCE_DIR}/src/style.css
    COMMENT "Building Node.js project to produce ${GZ_OUTPUT_FILE}"
    VERBATIM
)

# Create a target that depends on the output file. When this target is built,
# it ensures the custom command above is executed first.
add_custom_target(build_web_app ALL
    DEPENDS ${GZ_OUTPUT_FILE}
)


