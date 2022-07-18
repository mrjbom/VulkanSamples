#include <Windows.h>
#include "../../Base/source/BaseSample.h"
#include "../../Base/source/ErrorInfo/ErrorInfo.h"

class TriangleSample : public BaseSample
{
public:
    TriangleSample()
    {
        // Setting sample requirements
        base_title = "Triangle";
        //base_sampleInstanceRequirements.base_instanceEnabledExtensionsNames = { "NAME_OF_INSTANCE_EXTENSION1", "NAME_OF_INSTANCE_EXTENSION2" };
        //base_sampleDeviceRequirements.base_deviceEnabledExtensionsNames = { "NAME_OF_PHYSICAL_DEVICE_EXTENSION1", "NAME_OF_PHYSICAL_DEVICE_EXTENSION2" };
        //base_sampleDeviceRequirements.base_deviceRequiredQueueFamilyTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
    }

    ~TriangleSample()
    {
    }

    virtual bool getEnabledFeatures(VkPhysicalDevice physicalDevice)
    {
        return true;
    }
};

int main(int argc, char* argv[])
{
    try
    {
        TriangleSample* sample = new TriangleSample;
        sample->initVulkan();

        sample->finishVulkan();
        delete sample;
    }
    catch (std::exception ex)
    {
#ifdef _DEBUG
        std::cout << "Exception: " << ex.what() << std::endl;
#endif
        return EXIT_FAILURE;
    }
    catch (ErrorInfo errorInfo)
    {
#ifdef _DEBUG
        std::string errInfoStr = "Exception\n"
            + (std::string)"What: " + errorInfo.what + "\n"
            + (std::string)"File: " + errorInfo.file + "\n"
            + (std::string)"Line: " + errorInfo.line + "\n";
        std::cout << errInfoStr;
#elif
        wchar_t* what = new wchar_t[errorInfo.what.size()];
        mbstowcs(what, errorInfo.what.c_str(), errorInfo.what.size());
        MessageBox(NULL, what, L"Error", MB_OK);
        delete what;
#endif
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
