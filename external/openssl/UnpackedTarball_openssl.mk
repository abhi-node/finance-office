# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_UnpackedTarball_UnpackedTarball,openssl))

$(eval $(call gb_UnpackedTarball_set_tarball,openssl,$(OPENSSL_TARBALL),,openssl))

# 0001-const-up-ERR_str_libraries.patch upstream as:
#   https://github.com/openssl/openssl/pull/28029

$(eval $(call gb_UnpackedTarball_add_patches,openssl,\
	external/openssl/openssl-no-multilib.patch.0 \
	external/openssl/configurable-z-option.patch.0 \
	external/openssl/openssl-no-ipc-cmd.patch.0 \
	external/openssl/system-cannot-find-path-for-move.patch.0 \
	external/openssl/0001-const-up-ERR_str_libraries.patch \
))

# vim: set noet sw=4 ts=4:
