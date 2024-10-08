/*
 *  Copyright 2024 The Chromium Authors
 *  Use of this source code is governed by a BSD-style license that can be
 *  found in the LICENSE file.
 */
'use strict';

// Put variables in global scope to make them available to the browser console.
const constraints = window.constraints = {
  audio: false,
  video: true
};

/**
 * Handles successful retrieval of a media stream (usually video and/or audio).
 *
 * @param {!MediaStream} stream - The acquired MediaStream object.
 */
function handleSuccess(stream) {
  const video = document.querySelector('video');
  const videoTracks = stream.getVideoTracks();
  console.log('Got stream with constraints:', constraints);
  console.log(`Using video device: ${videoTracks[0].label}`);
  window.stream = stream; // make variable available to browser console
  video.srcObject = stream;
}

/**
 * Handles errors that occur during the process of acquiring user media
 * (camera and/or microphone) using the `getUserMedia` API.
 *
 * @param {!Error} error - The error object generated by the `getUserMedia`
 *                        failure.
 */
function handleError(error) {
  if (error.name === 'OverconstrainedError') {
    const v = constraints.video;
    errorMsg(`The resolution ${v.width.exact}x${v.height.exact} px ` +
      `is not supported by your device.`);
  } else if (error.name === 'NotAllowedError') {
    errorMsg('Permissions have not been granted to use your camera and ' +
      'microphone, you need to allow the page access to your devices in ' +
      'order for the demo to work.');
  }
  errorMsg(`getUserMedia error: ${error.name}`, error);
}

/**
 * Displays an error message within a designated HTML element and optionally
 * logs the error to the console for debugging.
 *
 * @param {string} msg - The error message to be displayed to the user.
 * @param {?Error} [error] - An optional Error object for console logging.
 */
function errorMsg(msg, error) {
  const errorElement = document.querySelector('#errorMsg');
  errorElement.innerHTML += `<p>${msg}</p>`;
  if (typeof error !== 'undefined') {
    console.error(error);
  }
}

/**
 * An asynchronous function that initializes media capture,
 * like a camera and/or microphone.
 *
 * @param {!Event} e - The event that triggered the initialization.
 */
async function init(e) {
  try {
    const stream = await navigator.mediaDevices.getUserMedia(constraints);
    handleSuccess(stream);
    e.target.disabled = true;
  } catch (e) {
    handleError(e);
  }
}

document.querySelector('#open-camera').addEventListener('click', e => init(e));
