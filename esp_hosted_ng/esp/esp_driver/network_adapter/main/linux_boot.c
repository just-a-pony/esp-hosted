/* Linux boot Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tiny_jffs2_reader.h"
#include "bootparam.h"
#include "linux_ll.h"

static const esp_partition_t *find_partition(const char *name)
{
	esp_partition_iterator_t it;
	const esp_partition_t *part;

	it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, name);
	if (!it)
		return NULL;
	part = esp_partition_get(it);
	return part;
}

#define CMDLINE_MAX 260

#ifdef CONFIG_LINUX_COMMAND_LINE
static void parse_cmdline(const void *ptr, uint32_t size, struct bp_tag tag[])
{
	struct jffs2_image img = {
		.data = (void *)ptr,
		.sz = size,
	};
	uint32_t cmdline_inode = jffs2_lookup(&img, 1, "cmdline");

	if (cmdline_inode) {
		char *cmdline = (char *)tag[1].data;
		size_t rd = jffs2_read(&img, cmdline_inode, cmdline, CMDLINE_MAX - 1);

		if (rd != -1) {
			tag[1].id = BP_TAG_COMMAND_LINE;
			cmdline[rd] = 0;
			ESP_LOGI(__func__, "found /etc/cmdline [%d] = '%s'\n", rd, cmdline);
		}
	}
}
#endif

static char IRAM_ATTR space_for_vectors[4096] __attribute__((aligned(4096)));

#define N_TAGS (3 + CMDLINE_MAX / sizeof(struct bp_tag))

static void map_flash_and_go(void)
{
	struct bp_tag tag[N_TAGS] = {
		[0] = {.id = BP_TAG_FIRST},
		[1] = {.id = BP_TAG_LAST, .size = CMDLINE_MAX},
		[N_TAGS - 1] = {.id = BP_TAG_LAST},
	};
	const esp_partition_t *part;
	const uint8_t *flash_base = (void *)0x42000000;
	const void *linux = NULL;
	const void *etc = NULL;

	init_linux_mmap();

	part = find_partition("linux");
	if (part) {
		linux = flash_base + part->address;
	}
#ifdef CONFIG_LINUX_COMMAND_LINE
	part = find_partition("etc");
	if (part) {
		etc = flash_base + part->address;
		parse_cmdline(etc, part->size, tag);
	}
#endif
	printf("linux ptr = %p\n", linux);
	printf("vectors ptr = %p\n", space_for_vectors);

	extern int g_abort_on_ipc;
	g_abort_on_ipc = 1;

	asm volatile ("mov a2, %1 ; jx %0" :: "r"(linux), "r"(tag) : "a2");
}

static void linux_task(void *p)
{
	map_flash_and_go();
	esp_restart();
}

void linux_boot(void)
{
	xTaskCreatePinnedToCore(linux_task, "linux_task", 4096, NULL, 5, NULL, 1);
}
