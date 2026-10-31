// Engine/SDL/DXVK-free control for #147. Driver unload and leak checks stay ordinary.
#include <vulkan/vulkan.h>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + " VkResult=" + std::to_string(result));
}

unsigned Number(const std::string& text, int base) {
    unsigned number = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), number, base);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || text.empty())
        throw std::runtime_error("Invalid numeric argument: " + text);
    return number;
}

struct Instance {
    VkInstance value = VK_NULL_HANDLE;
    ~Instance() { if (value) vkDestroyInstance(value, nullptr); }
};
struct Device {
    VkDevice value = VK_NULL_HANDLE;
    ~Device() { if (value) vkDestroyDevice(value, nullptr); }
};

void Maps(const std::filesystem::path& directory, const std::string& name) {
    if (directory.empty()) return;
    std::ifstream source("/proc/self/maps");
    std::ofstream destination(directory / (name + ".maps"));
    if (!source || !destination) throw std::runtime_error("Cannot record process maps");
    destination << source.rdbuf();
    destination.close();
    if (!destination) throw std::runtime_error("Cannot finish process maps");
}

std::vector<VkPhysicalDevice> PhysicalDevices(VkInstance instance) {
    for (unsigned retry = 0; retry < 4; ++retry) {
        uint32_t count = 0;
        Check(vkEnumeratePhysicalDevices(instance, &count, nullptr), "count physical devices");
        if (!count || count > 256) throw std::runtime_error("Physical device count outside 1..256");
        std::vector<VkPhysicalDevice> devices(count);
        const auto result = vkEnumeratePhysicalDevices(instance, &count, devices.data());
        if (result == VK_INCOMPLETE) continue;
        Check(result, "enumerate physical devices");
        devices.resize(count);
        if (devices.empty()) throw std::runtime_error("No physical devices");
        return devices;
    }
    throw std::runtime_error("Physical devices did not stabilize");
}

void LogicalDevice(const std::vector<VkPhysicalDevice>& devices, const std::string& selection) {
    VkPhysicalDevice selected = VK_NULL_HANDLE;
    unsigned vendor = 0, product = 0;
    if (!selection.empty()) {
        const auto colon = selection.find(':');
        if (colon == std::string::npos) throw std::runtime_error("Device must be hexadecimal vendor:device");
        vendor = Number(selection.substr(0, colon), 16);
        product = Number(selection.substr(colon + 1), 16);
    }
    for (const auto physical : devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physical, &properties);
        std::cout << "physical-device=" << properties.deviceName << " vendor=" << std::hex
                  << properties.vendorID << " device=" << properties.deviceID << std::dec
                  << " driver-version=" << properties.driverVersion
                  << " api-version=" << properties.apiVersion << '\n';
        if (selection.empty()) {
            if (!selected) selected = physical;
        } else if (properties.vendorID == vendor && properties.deviceID == product) {
            if (selected) throw std::runtime_error("Device selection is ambiguous");
            selected = physical;
        }
    }
    if (!selected) throw std::runtime_error("Requested physical device not found");
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(selected, &count, nullptr);
    if (!count || count > 256) throw std::runtime_error("Queue family count outside 1..256");
    std::vector<VkQueueFamilyProperties> queues(count);
    vkGetPhysicalDeviceQueueFamilyProperties(selected, &count, queues.data());
    queues.resize(count);
    for (uint32_t family = 0; family < queues.size(); ++family) {
        if (!queues[family].queueCount || !(queues[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
        const float priority = 1.0f;
        VkDeviceQueueCreateInfo queue{};
        queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue.queueFamilyIndex = family;
        queue.queueCount = 1;
        queue.pQueuePriorities = &priority;
        VkDeviceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        info.queueCreateInfoCount = 1;
        info.pQueueCreateInfos = &queue;
        Device device;
        Check(vkCreateDevice(selected, &info, nullptr, &device.value), "create device");
        Check(vkDeviceWaitIdle(device.value), "wait device idle");
        std::cout << "logical-device-created-and-idle queue-family=" << family << '\n';
        return;
    }
    throw std::runtime_error("No graphics queue family");
}
}

int main(int argc, char** argv) {
    try {
        std::cout << std::unitbuf;
        std::string stage = "device", selection;
        std::filesystem::path maps;
        unsigned cycles = 3;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (i + 1 == argc) throw std::runtime_error("Missing argument value");
            const std::string value = argv[++i];
            if (argument == "--stage") stage = value;
            else if (argument == "--cycles") cycles = Number(value, 10);
            else if (argument == "--device") selection = value;
            else if (argument == "--maps") maps = value;
            else throw std::runtime_error("Unknown argument: " + argument);
        }
        if (stage != "loader" && stage != "instance" && stage != "enumerate" && stage != "device")
            throw std::runtime_error("Stage must be loader, instance, enumerate or device");
        if (cycles < 1 || cycles > 32) throw std::runtime_error("Cycles must be 1..32");
        if (!selection.empty() && stage != "device") throw std::runtime_error("Device selection requires device stage");
        if (!selection.empty()) {
            const auto colon = selection.find(':');
            if (colon == std::string::npos) throw std::runtime_error("Device must be hexadecimal vendor:device");
            Number(selection.substr(0, colon), 16);
            Number(selection.substr(colon + 1), 16);
        }
        if (!maps.empty()) std::filesystem::create_directories(maps);
        std::cout << "vulkan-lifetime-control stage=" << stage << " cycles=" << cycles
                  << " header-version=" << VK_HEADER_VERSION << '\n';
        Maps(maps, "initial");
        uint32_t version = 0;
        Check(vkEnumerateInstanceVersion(&version), "loader version");
        if (version < VK_API_VERSION_1_1) throw std::runtime_error("Vulkan 1.1 loader required");
        std::cout << "loader-api-version=" << version << '\n';
        for (unsigned cycle = 0; cycle < cycles; ++cycle) {
            {
                Instance instance;
                if (stage != "loader") {
                    VkApplicationInfo application{};
                    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                    application.pApplicationName = "wowdecomp Vulkan lifetime control";
                    application.apiVersion = VK_API_VERSION_1_1;
                    VkInstanceCreateInfo info{};
                    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
                    info.pApplicationInfo = &application;
                    Check(vkCreateInstance(&info, nullptr, &instance.value), "create instance");
                    if (stage == "enumerate" || stage == "device") {
                        const auto devices = PhysicalDevices(instance.value);
                        std::cout << "physical-device-count=" << devices.size() << '\n';
                        if (stage == "device") LogicalDevice(devices, selection);
                    }
                }
                Maps(maps, "before-destroy-" + std::to_string(cycle));
            }
            Maps(maps, "after-destroy-" + std::to_string(cycle));
            std::cout << "cycle=" << cycle + 1 << " destroyed\n";
        }
        std::cout << "Vulkan lifetime assertions passed; process-exit leak result follows\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Vulkan lifetime control failed: " << error.what() << '\n';
        return 1;
    }
}
