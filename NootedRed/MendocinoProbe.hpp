// Read-only Mendocino identification and native X6000 load observation.
#pragma once
class IOPCIDevice;
namespace MendocinoProbe {
enum class Component { Framebuffer, Accelerator, HWLibs };
void publish(IOPCIDevice* device, unsigned int pciRevision);
void noteX6000Component(Component component);
}
