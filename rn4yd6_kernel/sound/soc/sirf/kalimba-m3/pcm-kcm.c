/*
 * SiRF Kalimba pcm audio driver
 *
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "kcm/kcm.h"
#include "kcm/kasobj.h"
#include "audio-protocol.h"

struct kas_pcm_data {
	struct snd_pcm_substream *substream;
	u32 rate;
	u32 channels;
	u32 period_bytes;
	u32 buff_bytes;
	u32 buff_addr;
	u32 pos;
	bool internal;
};

#define KAS_PCM_COUNT	32
struct kas_pcm_data pcm_data[KAS_PCM_COUNT];

static const struct snd_pcm_hardware kas_pcm_hardware = {
	.info = SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_PAUSE |
		SNDRV_PCM_INFO_RESUME |
		SNDRV_PCM_INFO_BLOCK_TRANSFER,
	.formats = SNDRV_PCM_FMTBIT_S16_LE |
		SNDRV_PCM_FMTBIT_S24_LE |
		SNDRV_PCM_FMTBIT_S32_LE,
	.period_bytes_min	= 32,
	.period_bytes_max	= 256 * 1024,
	.periods_min		= 2,
	.periods_max		= 128,
	.buffer_bytes_max	= 512 * 1024, /* 512 kbytes */
};

#define KAS_RATES		(SNDRV_PCM_RATE_CONTINUOUS | \
				SNDRV_PCM_RATE_8000_192000)
#define KAS_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE | \
				SNDRV_PCM_FMTBIT_S24_LE)

int kas_pcm_notify(u32 stream, u32 pos)
{
	pcm_data[stream].pos = pos;
	if (pcm_data[stream].substream && !pcm_data[stream].internal)
		snd_pcm_period_elapsed(pcm_data[stream].substream);

	return 0;
}

static int kas_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct kasobj_fe *fe = kcm_find_fe(rtd->cpu_dai->driver->name,
				substream->stream == SNDRV_PCM_STREAM_PLAYBACK);

	if (fe)
		pcm_data[rtd->cpu_dai->id].internal = fe->db->internal;
	else
		pcm_data[rtd->cpu_dai->id].internal = true;

	pcm_data[rtd->cpu_dai->id].substream = substream;

	snd_soc_set_runtime_hwparams(substream, &kas_pcm_hardware);
	return snd_pcm_hw_constraint_integer(substream->runtime,
		SNDRV_PCM_HW_PARAM_PERIODS);
}

static int kas_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int rate = params_rate(params);
	unsigned int channels = params_channels(params);
	u32 period_bytes = params_period_bytes(params);
	u32 buff_bytes = params_buffer_bytes(params);
	u32 buff_addr;
	int ret;

	pcm_data[rtd->cpu_dai->id].pos = 0;

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
	if (ret < 0) {
		dev_err(rtd->dev, "allocate %d bytes for PCM failed: %d\n",
			params_buffer_bytes(params), ret);
		return ret;
	}
	buff_addr = runtime->dma_addr;

	/* FIXME: Remve this when find solution */
	if (kcm_strcasestr(rtd->cpu_dai->driver->name, "Tunex")) {
		struct kas_pcm_data *pdata = &pcm_data[rtd->cpu_dai->id];

		pdata->rate = rate;
		pdata->channels = channels;
		pdata->period_bytes = period_bytes;
		pdata->buff_bytes = buff_bytes;
		pdata->buff_addr = buff_addr;
		return 0;
	}

	ret = kas_create_stream(rtd->cpu_dai->id, rate, channels, buff_addr,
		buff_bytes, period_bytes);

	return ret;
}

static int kas_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int channels = runtime->channels;
	int ret = 0;

	/* FIXME: Remve this when find solution */
	if (!kcm_strcasestr(rtd->cpu_dai->driver->name, "Tunex"))
		ret = kas_destroy_stream(rtd->cpu_dai->id, channels);
	pcm_data[rtd->cpu_dai->id].substream = NULL;
	snd_pcm_lib_free_pages(substream);

	return ret;
}

static int kas_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct kas_pcm_data *p = &pcm_data[rtd->cpu_dai->id];
	int ret;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* FIXME: Remve this when find solution */
		if (kcm_strcasestr(rtd->cpu_dai->driver->name, "Tunex")) {
			ret = kas_create_stream(rtd->cpu_dai->id, p->rate,
				p->channels, p->buff_addr, p->buff_bytes,
				p->period_bytes);
			if (ret)
				return ret;
		}
		ret = kas_start_stream(rtd->cpu_dai->id);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		ret = kas_stop_stream(rtd->cpu_dai->id);
		if (ret)
			return ret;
		/* FIXME: Remve this when find solution */
		if (kcm_strcasestr(rtd->cpu_dai->driver->name, "Tunex"))
			ret = kas_destroy_stream(rtd->cpu_dai->id, p->channels);
		break;
	default:
		ret = 0;
	}

	return ret;
}

static snd_pcm_uframes_t kas_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	return bytes_to_frames(substream->runtime,
		pcm_data[rtd->cpu_dai->id].pos);
}

static struct snd_pcm_ops kas_pcm_ops = {
	.open = kas_pcm_open,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = kas_pcm_hw_params,
	.hw_free = kas_pcm_hw_free,
	.trigger = kas_pcm_trigger,
	.pointer = kas_pcm_pointer,
};

static int kas_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_card *card = rtd->card->snd_card;
	int ret = 0;

	ret = dma_coerce_mask_and_coherent(card->dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	pcm->nonatomic = true;
	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream ||
			pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = snd_pcm_lib_preallocate_pages_for_all(pcm,
				SNDRV_DMA_TYPE_DEV_IRAM,
				rtd->card->snd_card->dev,
				kas_pcm_hardware.buffer_bytes_max,
				kas_pcm_hardware.buffer_bytes_max);
		if (ret) {
			dev_err(rtd->dev, "dma buffer allocation failed %d\n",
					ret);
			return ret;
		}
	}

	return ret;
}

static void kas_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int kas_pcm_probe(struct snd_soc_platform *platform)
{
	struct snd_kcontrol_new *ctrl;

	/* Register all control interfaces */
	ctrl = kcm_ctrl_first();
	while (ctrl) {
		snd_soc_add_platform_controls(platform, ctrl, 1);
		ctrl = kcm_ctrl_next();
	}

	return 0;
}

static struct snd_soc_platform_driver kas_soc_platform = {
	.probe = kas_pcm_probe,
	.ops = &kas_pcm_ops,
	.pcm_new = kas_pcm_new,
	.pcm_free = kas_pcm_free,
};


static struct snd_soc_component_driver kas_dai_component = {
	.name = "kas-dai",
};

static int kas_pcm_dev_probe(struct platform_device *pdev)
{
	int ret, cpu_dai_cnt, route_cnt, widget_cnt;
	struct snd_soc_dai_driver *cpu_dai;
	struct snd_soc_dapm_widget *widget;
	struct snd_soc_dapm_route *route;


	ret = kcm_drv_status();
	if (ret)
		return ret;

	kcm_set_dev(&pdev->dev);

	/* Get CPU DAI and DAPM route table */
	cpu_dai = (struct snd_soc_dai_driver *)kcm_get_dai(&cpu_dai_cnt);
	widget =
		(struct snd_soc_dapm_widget *)kcm_get_codec_widget(&widget_cnt);
	route = (struct snd_soc_dapm_route *)kcm_get_route(&route_cnt);
	kas_dai_component.dapm_widgets = widget;
	kas_dai_component.num_dapm_widgets = widget_cnt;
	kas_dai_component.dapm_routes = route,
	kas_dai_component.num_dapm_routes = route_cnt;

	ret = devm_snd_soc_register_platform(&pdev->dev, &kas_soc_platform);
	if (ret < 0)
		return ret;

	return devm_snd_soc_register_component(&pdev->dev, &kas_dai_component,
			cpu_dai, cpu_dai_cnt);
}

static const struct of_device_id kas_pcm_of_match[] = {
	{ .compatible = "csr,kas-pcm", },
	{}
};
MODULE_DEVICE_TABLE(of, kas_pcm_of_match);

static struct platform_driver kas_pcm_driver = {
	.driver = {
		.name = "kas-pcm-audio",
		.owner = THIS_MODULE,
		.of_match_table = kas_pcm_of_match,
	},
	.probe = kas_pcm_dev_probe,
};
module_platform_driver(kas_pcm_driver);

MODULE_DESCRIPTION("SiRF Kalimba pcm audio driver");
