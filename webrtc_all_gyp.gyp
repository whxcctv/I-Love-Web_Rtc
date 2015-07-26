# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'webrtc_all_gyp',
      'type': 'none',
      'dependencies': [
        'webrtc.gyp:*',
        'libjingle/libjingle.gyp:*',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            'webrtc_examples.gyp:*',
            'platform_android.gyp:*',
          ],
        }],
      ],
    },
  ],
}