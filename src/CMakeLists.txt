cmake_minimum_required(VERSION 3.25)
project(TailoredSketch_Code)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Sketch)

set(SKETCH_HEADERS
        Sketch/Sketch.h
        Sketch/CM_Sketch.h
        Sketch/CU_Sketch.h
        Sketch/CountLess.h
        Sketch/StingyC.h
        Sketch/StingyCM.h
        Sketch/StingyCM_Base.h
        Sketch/StingyCM_Sample.h
        Sketch/StingyCU.h
        Sketch/StingyCU_Base.h
        Sketch/Tailored.h
        Sketch/O_Tailored.h
        Sketch/C_Sketch.h
        Sketch/KK_Sketch.h
        Sketch/Stable_Sketch.h
        Sketch/LightGuardian_Sketch.h
        Sketch/Elastic_Sketch.h
        Sketch/UnivMon_Sketch.h
        Sketch/Nitro_Sketch.h
        Sketch/SketchVisor.h)

add_executable(TailoredSketch_Code main.cpp Choose_Ske.h MurmurHash.h params.h ${SKETCH_HEADERS})
add_executable(KKSketch_Benchmark kk_benchmark.cpp Choose_Ske.h MurmurHash.h params.h ${SKETCH_HEADERS})
