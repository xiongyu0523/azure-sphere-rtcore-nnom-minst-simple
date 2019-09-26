#  Copyright (c) Microsoft Corporation. All rights reserved.
#  Licensed under the MIT License.

CMAKE_MINIMUM_REQUIRED(VERSION 3.11)
PROJECT(Azure_Sphere_RTcore_FreeRTOS C)

# include
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/freertos/include ${CMAKE_SOURCE_DIR}/freertos/portable ${CMAKE_SOURCE_DIR}/printf ${CMAKE_SOURCE_DIR}/nnom/port  ${CMAKE_SOURCE_DIR}/nnom/inc)

# Create executable
ADD_EXECUTABLE(${PROJECT_NAME} main.c 
							   freertos/list.c freertos/tasks.c freertos/queue.c freertos/event_groups.c freertos/timers.c freertos/stream_buffer.c freertos/portable/heap_4.c freertos/portable/port.c 
			                   printf/printf.c
							   nnom/src/backends/nnom_local.c nnom/src/core/nnom.c nnom/src/core/nnom_layers.c nnom/src/core/nnom_tensor.c nnom/src/core/nnom_utils.c nnom/src/layers/nnom_activation.c nnom/src/layers/nnom_avgpool.c nnom/src/layers/nnom_baselayer.c nnom/src/layers/nnom_concat.c nnom/src/layers/nnom_conv2d.c nnom/src/layers/nnom_cropping.c nnom/src/layers/nnom_dense.c nnom/src/layers/nnom_dw_conv2d.c nnom/src/layers/nnom_flatten.c nnom/src/layers/nnom_global_pool.c nnom/src/layers/nnom_input.c nnom/src/layers/nnom_lambda.c nnom/src/layers/nnom_matrix.c nnom/src/layers/nnom_maxpool.c nnom/src/layers/nnom_output.c nnom/src/layers/nnom_rnn.c nnom/src/layers/nnom_softmax.c nnom/src/layers/nnom_sumpool.c nnom/src/layers/nnom_upsample.c nnom/src/layers/nnom_zero_padding.c
							   )
TARGET_LINK_LIBRARIES(${PROJECT_NAME})
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES LINK_DEPENDS ${CMAKE_SOURCE_DIR}/linker.ld)

# Add MakeImage post-build command
INCLUDE("${AZURE_SPHERE_MAKE_IMAGE_FILE}")
