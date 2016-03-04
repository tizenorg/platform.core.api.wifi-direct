/*
 * Wi-Fi Direct
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/**
 * This file declares macros for logging.
 *
 * @file        wifi-direct-log.h
 * @author      Nishant Chaprana (n.chaprana@samsung.com)
 * @version     0.1
 */

#ifndef __WIFI_DIRECT_LOG_H_
#define __WIFI_DIRECT_LOG_H_

#ifdef USE_DLOG
#include <dlog.h>

#undef LOG_TAG
#define LOG_TAG "WIFI_DIRECT"

#define WDC_LOGV(format, args...) LOGV(format, ##args)
#define WDC_LOGD(format, args...) LOGD(format, ##args)
#define WDC_LOGI(format, args...) LOGI(format, ##args)
#define WDC_LOGW(format, args...) LOGW(format, ##args)
#define WDC_LOGE(format, args...) LOGE(format, ##args)
#define WDC_LOGF(format, args...) LOGF(format, ##args)

#define WDC_SECLOGI(format, args...) SECURE_LOG(LOG_INFO, LOG_TAG, format, ##args)
#define WDC_SECLOGD(format, args...) SECURE_LOG(LOG_DEBUG, LOG_TAG, format, ##args)

#define __WDC_LOG_FUNC_START__ LOGV("Enter")
#define __WDC_LOG_FUNC_END__ LOGV("Quit")

#else /** _DLOG_UTIL */

#define WDC_LOGV(format, args...)
#define WDC_LOGD(format, args...)
#define WDC_LOGI(format, args...)
#define WDC_LOGW(format, args...)
#define WDC_LOGE(format, args...)
#define WDC_LOGF(format, args...)

#define __WDC_LOG_FUNC_START__
#define __WDC_LOG_FUNC_END__

#define WDC_SECLOGI(format, args...)
#define WDC_SECLOGD(format, args...)

#endif /** _DLOG_UTIL */
#endif /** __WIFI_DIRECT_LOG_H_ */
