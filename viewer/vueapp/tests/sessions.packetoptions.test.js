'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import '@testing-library/jest-dom';
import { render, fireEvent } from '@testing-library/vue';
import userEvent from '@testing-library/user-event';
import PacketOptions from '../src/components/sessions/PacketOptions.vue';
import { sessions } from '../../../common/vueapp/tests/consts';

global.$ = global.jQuery = $;

Vue.use(BootstrapVue);

const props = {
  params: {
    line: true,
    base: 'hex',
    gzip: false,
    image: false,
    packets: '50',
    showDst: true,
    showSrc: true,
    showFrames: false,
    decode: { 'BODY-UNXOR': { keyLength: '16', skip: '12' } }
  },
  decodings: {},
  cyberChefSrcUrl: `cyberchef.html?nodeId=test&sessionId=${sessions[0].id}&type=src`,
  cyberChefDstUrl: `cyberchef.html?nodeId=test&sessionId=${sessions[0].id}&type=dst`
};

test('sessions - packet options', async () => {
  const {
    getByTitle, queryByTitle, getAllByRole, getByPlaceholderText, emitted, updateProps
  } = render(PacketOptions, {
    props
  });

  // change num packets emits updateNumPackets with new value -------------- //
  const numPacketsSelect = getAllByRole('listbox')[0];
  await userEvent.selectOptions(numPacketsSelect, '1000');
  expect(emitted()).toHaveProperty('updateNumPackets');
  expect(emitted().updateNumPackets[0][0]).toBe(1000);

  // change base emits updateBase with new value --------------------------- //
  const baseSelect = getAllByRole('listbox')[1];
  await userEvent.selectOptions(baseSelect, 'natural');
  expect(emitted()).toHaveProperty('updateBase');
  expect(emitted().updateBase[0][0]).toBe('natural');

  // packet options dropdown exists and can be opened ---------------------- //
  const optionsDropdown = getByTitle('Packet Options');
  await fireEvent.click(optionsDropdown);

  // click show frames toggle emits toggleShowFrames ----------------------- //
  const showFramesToggle = getByTitle('Show Raw Packets');
  await fireEvent.click(showFramesToggle);
  expect(emitted()).toHaveProperty('toggleShowFrames');

  // click packet info toggle emits toggleTimestamps ----------------------- //
  const packetInfoToggle = getByTitle('Show Packet Info');
  await fireEvent.click(packetInfoToggle);
  expect(emitted()).toHaveProperty('toggleTimestamps');

  // click line numbers toggle emits toggleLineNumbers --------------------- //
  const lineNumbersToggle = getByTitle('Hide Line Numbers');
  await fireEvent.click(lineNumbersToggle);
  expect(emitted()).toHaveProperty('toggleLineNumbers');

  // click compressing toggle emits toggleCompression ---------------------- //
  const compressingToggle = getByTitle(/Enable Uncompressing/);
  await fireEvent.click(compressingToggle);
  expect(emitted()).toHaveProperty('toggleCompression');

  // click image toggle emits imagesToggle --------------------------------- //
  const imagesToggle = getByTitle(/Show Images & Files/);
  await fireEvent.click(imagesToggle);
  expect(emitted()).toHaveProperty('toggleImages');

  // click src toggle emits toggleShowSrc ---------------------------------- //
  const srcToggle = getByTitle('Toggle source packet visibility');
  expect(srcToggle).toHaveClass('active');
  await fireEvent.click(srcToggle);
  expect(emitted()).toHaveProperty('toggleShowSrc');

  // click dst toggle emits toggleShowDst ---------------------------------- //
  const dstToggle = getByTitle('Toggle destination packet visibility');
  expect(dstToggle).toHaveClass('active');
  await fireEvent.click(dstToggle);
  expect(emitted()).toHaveProperty('toggleShowDst');

  // update props updates titles/text in dropdown menu --------------------- //
  await updateProps({
    params: {
      ts: true,
      line: false,
      base: 'ascii', // line numbers should only be availble for 'hex'
      gzip: true,
      image: true,
      packets: '200',
      showDst: false,
      showSrc: false,
      showFrames: false,
      decode: { 'BODY-UNXOR': { keyLength: '16', skip: '12' } }
    },
    decodings: {},
    cyberChefSrcUrl: `cyberchef.html?nodeId=test&sessionId=${sessions[0].id}&type=src`,
    cyberChefDstUrl: `cyberchef.html?nodeId=test&sessionId=${sessions[0].id}&type=dst`
  });

  // gzip should disable the num packets select and display why
  expect(numPacketsSelect).toHaveClass('disabled');
  getByTitle(/You cannot select the number of packets/);

  // updated values
  expect(numPacketsSelect.value).toBe('200');
  expect(getAllByRole('listbox')[1].value).toBe('ascii');

  // updated titles
  getByTitle('Show Raw Packets');
  getByTitle('Hide Packet Info');
  expect(queryByTitle('Show Line Numbers')).not.toBeInTheDocument();
  expect(queryByTitle('Hide Line Numbers')).not.toBeInTheDocument();
  getByTitle('Disable Uncompressing');
  getByTitle('Hide Images & Files');
  expect(getByTitle('Toggle source packet visibility')).not.toHaveClass('active');
  expect(getByTitle('Toggle destination packet visibility')).not.toHaveClass('active');

  // if showFrames is true, can't toggle uncompressing and images & files -- /
  await updateProps({
    params: {
      ts: true,
      line: false,
      base: 'ascii',
      gzip: true,
      image: false,
      packets: '200',
      showDst: false,
      showSrc: false,
      showFrames: true,
      decode: { 'BODY-UNXOR': { keyLength: '16', skip: '12' } }
    },
    decodings: {
      'BODY-UNBASE64': { active: false, name: 'Unbase64' },
      'BODY-UNXOR': {
        active: false,
        name: 'UnXOR',
        title: 'Only set keyLength or key',
        fields: [{
          key: 'skip',
          type: 'text',
          name: 'Skip Bytes'
        }, {
          type: 'text',
          key: 'keyLength',
          name: 'Key is in data length'
        }, {
          key: 'key',
          type: 'text',
          name: 'Fixed key in hex'
        }]
      }
    },
    cyberChefSrcUrl: `cyberchef.html?nodeId=test&sessionId=${sessions[0].id}&type=src`,
    cyberChefDstUrl: `cyberchef.html?nodeId=test&sessionId=${sessions[0].id}&type=dst`
  });

  expect(queryByTitle('Disable Uncompressing')).not.toBeInTheDocument();
  expect(queryByTitle(/Enable Uncompressing/)).not.toBeInTheDocument();
  expect(queryByTitle(/Show Images & Files/)).not.toBeInTheDocument();
  expect(queryByTitle('Hide Images & Files')).not.toBeInTheDocument();

  // toggle decoding without form ------------------------------------------ //
  let decodingBtn = getByTitle('Toggle Unbase64 Decoding');
  await fireEvent.click(decodingBtn);
  expect(emitted()).toHaveProperty('updateDecodings');
  expect(emitted().updateDecodings[0][0]).toStrictEqual(expect.objectContaining({
    'BODY-UNBASE64': {
      active: true,
      name: 'Unbase64'
    }
  }));

  // toggle decoding with form --------------------------------------------- //
  decodingBtn = getByTitle('Toggle UnXOR Decoding');
  await fireEvent.click(decodingBtn);
  expect(emitted()).toHaveProperty('updateDecodings');
  expect(emitted().updateDecodings[0][0]).toStrictEqual(expect.objectContaining({
    'BODY-UNXOR': expect.objectContaining({
      active: true,
      name: 'UnXOR'
    })
  }));

  // decoding form opens and closes ---------------------------------------- //
  getByPlaceholderText('Key is in data length');
  const cancelDecoding = getByTitle('cancel');
  await fireEvent.click(cancelDecoding);
  expect(queryByTitle('apply')).not.toBeInTheDocument();
});
