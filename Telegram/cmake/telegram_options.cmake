# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

set(TDESKTOP_API_ID 17349)
set(TDESKTOP_API_HASH 344583e45741c457fe1862106095a5eb)

if (DESKTOP_APP_DISABLE_AUTOUPDATE)
    target_compile_definitions(Telegram PRIVATE TDESKTOP_DISABLE_AUTOUPDATE)
endif()

if (DESKTOP_APP_SPECIAL_TARGET)
    target_compile_definitions(Telegram PRIVATE TDESKTOP_ALLOW_CLOSED_ALPHA)
endif()
