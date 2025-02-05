#define OCL_CHECK(error, call)                                                                   \
        call;                                                                                        \
        if (error != CL_SUCCESS) {                                                                   \
            printf("%s:%d Error calling " #call ", error code is: %d\n", __FILE__, __LINE__, error); \
            exit(EXIT_FAILURE);                                                                      \
        }

// OpenCL utility layer include
#include "xcl2/xcl2.hpp"
#include <algorithm>
#include <vector>
#define DATA_SIZE 4096

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <XCLBIN File>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string binaryFile = argv[1];
    size_t vector_size_bytes = sizeof(int) * DATA_SIZE;
    cl_int err;
    cl::Context context;
    cl::Kernel krnl_vector_add_1,krnl_vector_add_2,krnl_vector_add_3;
    cl::CommandQueue q;
    // Allocate Memory in Host Memory
    // When creating a buffer with user pointer (CL_MEM_USE_HOST_PTR), under the
    // hood user ptr
    // is used if it is properly aligned. when not aligned, runtime had no choice
    // but to create
    // its own host side buffer. So it is recommended to use this allocator if
    // user wish to
    // create buffer using CL_MEM_USE_HOST_PTR to align user buffer to page
    // boundary. It will
    // ensure that user buffer is used when user create Buffer/Mem object with
    // CL_MEM_USE_HOST_PTR
    std::vector<int, aligned_allocator<int> > source_in1(DATA_SIZE);
    std::vector<int, aligned_allocator<int> > source_in2(DATA_SIZE);
    std::vector<int, aligned_allocator<int> > source_hw_results(DATA_SIZE);
    std::vector<int, aligned_allocator<int> > source_sw_results(DATA_SIZE);

    // Create the test data
    std::generate(source_in1.begin(), source_in1.end(), std::rand);
    std::generate(source_in2.begin(), source_in2.end(), std::rand);
    for (int i = 0; i < DATA_SIZE; i++) {
        source_sw_results[i] = source_in1[i] + source_in2[i];
        source_hw_results[i] = 0;
    }

    // OPENCL HOST CODE AREA START
    // get_xil_devices() is a utility API which will find the xilinx
    // platforms and will return list of devices connected to Xilinx platform
    auto devices = xcl::get_xil_devices();
    // read_binary_file() is a utility API which will load the binaryFile
    // and will return the pointer to file buffer.
    auto fileBuf = xcl::read_binary_file(binaryFile);
    cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};
    bool valid_device = false;
    for (unsigned int i = 0; i < devices.size(); i++) {
        auto device = devices[i];
        // Creating Context and Command Queue for selected Device
        OCL_CHECK(err, context = cl::Context(device, nullptr, nullptr, nullptr, &err));
        OCL_CHECK(err, q = cl::CommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err));
        std::cout << "Trying to program device[" << i << "]: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
        cl::Program program(context, {device}, bins, nullptr, &err);
        if (err != CL_SUCCESS) {
            std::cout << "Failed to program device[" << i << "] with xclbin file!\n";
        } else {
            std::cout << "Device[" << i << "]: program successful!\n";
            OCL_CHECK(err, krnl_vector_add_1 = cl::Kernel(program, "vadd:{vadd_1}", &err));
            OCL_CHECK(err, krnl_vector_add_2 = cl::Kernel(program, "vadd:{vadd_2}", &err));
            OCL_CHECK(err, krnl_vector_add_3 = cl::Kernel(program, "vadd:{vadd_3}", &err));
            valid_device = true;
            break; // we break because we found a valid device
        }
    }
    if (!valid_device) {
        std::cout << "Failed to program any device found, exit!\n";
        exit(EXIT_FAILURE);
    }

    // Allocate Buffer in Global Memory
    // Buffers are allocated using CL_MEM_USE_HOST_PTR for efficient memory and
    // Device-to-host communication
    OCL_CHECK(err, cl::Buffer buffer_in1_vadd_1(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, vector_size_bytes,
                                         source_in1.data(), &err));
    OCL_CHECK(err, cl::Buffer buffer_in2_vadd_1(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, vector_size_bytes,
                                         source_in2.data(), &err));
    OCL_CHECK(err, cl::Buffer buffer_output_vadd_1(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, vector_size_bytes,
                                            source_hw_results.data(), &err));
    OCL_CHECK(err, cl::Buffer buffer_in1_vadd_2(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, vector_size_bytes,
                                         source_in1.data(), &err));
    OCL_CHECK(err, cl::Buffer buffer_in2_vadd_2(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, vector_size_bytes,
                                         source_in2.data(), &err));
    OCL_CHECK(err, cl::Buffer buffer_output_vadd_2(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, vector_size_bytes,
                                            source_hw_results.data(), &err));
    OCL_CHECK(err, cl::Buffer buffer_in1_vadd_3(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, vector_size_bytes,
                                         source_in1.data(), &err));
    OCL_CHECK(err, cl::Buffer buffer_in2_vadd_3(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, vector_size_bytes,
                                         source_in2.data(), &err));
    OCL_CHECK(err, cl::Buffer buffer_output_vadd_3(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, vector_size_bytes,
                                            source_hw_results.data(), &err));

    int size = DATA_SIZE;
    OCL_CHECK(err, err = krnl_vector_add_1.setArg(0, buffer_in1_vadd_1));
    OCL_CHECK(err, err = krnl_vector_add_1.setArg(1, buffer_in2_vadd_2));
    OCL_CHECK(err, err = krnl_vector_add_1.setArg(2, buffer_output_vadd_1));
    OCL_CHECK(err, err = krnl_vector_add_1.setArg(3, size));

    OCL_CHECK(err, err = krnl_vector_add_2.setArg(0, buffer_in1_vadd_2));
    OCL_CHECK(err, err = krnl_vector_add_2.setArg(1, buffer_in2_vadd_2));
    OCL_CHECK(err, err = krnl_vector_add_2.setArg(2, buffer_output_vadd_2));
    OCL_CHECK(err, err = krnl_vector_add_2.setArg(3, size));

    OCL_CHECK(err, err = krnl_vector_add_3.setArg(0, buffer_in1_vadd_3));
    OCL_CHECK(err, err = krnl_vector_add_3.setArg(1, buffer_in2_vadd_3));
    OCL_CHECK(err, err = krnl_vector_add_3.setArg(2, buffer_output_vadd_3));
    OCL_CHECK(err, err = krnl_vector_add_3.setArg(3, size));

    // Copy input data to device global memory
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_in1_vadd_1, buffer_in2_vadd_1}, 0 /* 0 means from host*/));
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_in1_vadd_2, buffer_in2_vadd_2}, 0 /* 0 means from host*/));
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_in1_vadd_3, buffer_in2_vadd_3}, 0 /* 0 means from host*/));

    // Launch the Kernel
    // For HLS kernels global and local size is always (1,1,1). So, it is
    // recommended
    // to always use enqueueTask() for invoking HLS kernel
    OCL_CHECK(err, err = q.enqueueTask(krnl_vector_add_1));
    OCL_CHECK(err, err = q.enqueueTask(krnl_vector_add_2));
    OCL_CHECK(err, err = q.enqueueTask(krnl_vector_add_3));

    // Copy Result from Device Global Memory to Host Local Memory
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_output_vadd_1}, CL_MIGRATE_MEM_OBJECT_HOST));
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_output_vadd_2}, CL_MIGRATE_MEM_OBJECT_HOST));
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_output_vadd_3}, CL_MIGRATE_MEM_OBJECT_HOST));
    q.finish();
    // OPENCL HOST CODE AREA END

    // Compare the results of the Device to the simulation
    bool match = true;
    for (int i = 0; i < DATA_SIZE; i++) {
        if (source_hw_results[i] != source_sw_results[i]) {
            std::cout << "Error: Result mismatch" << std::endl;
            std::cout << "i = " << i << " CPU result = " << source_sw_results[i]
                      << " Device result = " << source_hw_results[i] << std::endl;
            match = false;
            break;
        }
    }

    std::cout << "TEST " << (match ? "PASSED" : "FAILED") << std::endl;
    return (match ? EXIT_SUCCESS : EXIT_FAILURE);
}