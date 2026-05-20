/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef VULKAN_BENCH_WINDOW_H
#define VULKAN_BENCH_WINDOW_H


#include <Messenger.h>
#include <StringView.h>
#include <Window.h>


enum {
	kMsgVkBenchResult	= 'vkrs',
	kMsgVkBenchStart	= 'vkst'
};


static const int32 kNumVkTests = 4;


struct VulkanBenchResults {
	float	scores[kNumVkTests];
	float	overallScore;
	BString	deviceName;
	BString	driverInfo;
	bool	valid;
};


class VulkanBenchWindow : public BWindow {
public:
								VulkanBenchWindow(BMessenger target);
	virtual						~VulkanBenchWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			void				_RunBenchmark();
			bool				_InitVulkan();
			void				_Cleanup();

			float				_TestMemoryBandwidth();
			float				_TestComputeShader();
			float				_TestBufferCopy();
			float				_TestFillRate();

			void				_UpdateLabels();

	static	int32				_BenchThread(void* data);

			BMessenger			fTarget;
			BStringView*		fDeviceLabel;
			BStringView*		fDriverLabel;
			BStringView*		fTestLabels[kNumVkTests];
			BStringView*		fStatusLabel;
			BStringView*		fScoreLabel;
			thread_id			fBenchThread;

			VulkanBenchResults	fResults;

			// Vulkan handles (opaque — cast from VkFoo)
			void*				fInstance;
			void*				fPhysicalDevice;
			void*				fDevice;
			void*				fCommandPool;
			void*				fCommandBuffer;
			uint32				fQueueFamily;
			void*				fQueue;
};


#endif // VULKAN_BENCH_WINDOW_H
