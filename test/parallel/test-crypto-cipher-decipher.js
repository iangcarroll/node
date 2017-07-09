'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
if (common.hasFipsCrypto) {
  common.skip('not supported in FIPS mode');
  return;
}
const crypto = require('crypto');
const assert = require('assert');

function testCipher1(key) {
  key = crypto.generateLegacyKey('aes-192-cbc', key);
  const iv = crypto.generateLegacyIV('aes-192-cbc', key);

  // Test encryption and decryption
  const plaintext = 'Keep this a secret? No! Tell everyone about node.js!';

  const cipher = crypto.createCipheriv('aes-192-cbc', key, iv);

  // encrypt plaintext which is in utf8 format
  // to a ciphertext which will be in hex
  let ciph = cipher.update(plaintext, 'utf8', 'hex');
  // Only use binary or hex, not base64.
  ciph += cipher.final('hex');

  const decipher = crypto.createDecipheriv('aes-192-cbc', key, iv);
  let txt = decipher.update(ciph, 'hex', 'utf8');
  txt += decipher.final('utf8');

  assert.strictEqual(txt, plaintext, 'encryption and decryption');

  // streaming cipher interface
  // NB: In real life, it's not guaranteed that you can get all of it
  // in a single read() like this.  But in this case, we know it's
  // quite small, so there's no harm.
  const cStream = crypto.createCipheriv('aes-192-cbc', key, iv);
  cStream.end(plaintext);
  ciph = cStream.read();

  const dStream = crypto.createDecipheriv('aes-192-cbc', key, iv);
  dStream.end(ciph);
  txt = dStream.read().toString('utf8');

  assert.strictEqual(txt, plaintext, 'encryption and decryption with streams');
}


function testCipher2(key) {
  // encryption and decryption with Base64
  // reported in https://github.com/joyent/node/issues/738
  key = crypto.generateLegacyKey('aes-256-cbc', key);
  const iv = crypto.generateLegacyIV('aes-256-cbc', key);

  const plaintext =
      '32|RmVZZkFUVmpRRkp0TmJaUm56ZU9qcnJkaXNNWVNpTTU*|iXmckfRWZBGWWELw' +
      'eCBsThSsfUHLeRe0KCsK8ooHgxie0zOINpXxfZi/oNG7uq9JWFVCk70gfzQH8ZUJ' +
      'jAfaFg**';
  const cipher = crypto.createCipheriv('aes-256-cbc', key, iv);

  // encrypt plaintext which is in utf8 format
  // to a ciphertext which will be in Base64
  let ciph = cipher.update(plaintext, 'utf8', 'base64');
  ciph += cipher.final('base64');

  const decipher = crypto.createDecipheriv('aes-256-cbc', key, iv);
  let txt = decipher.update(ciph, 'base64', 'utf8');
  txt += decipher.final('utf8');

  assert.strictEqual(txt, plaintext, 'encryption and decryption with Base64');
}

testCipher1('MySecretKey123');
testCipher1(Buffer.from('MySecretKey123'));

testCipher2('0123456789abcdef');
testCipher2(Buffer.from('0123456789abcdef'));

// Base64 padding regression test, see #4837.
{
  const key = crypto.generateLegacyKey('aes-256-cbc', 'secret');
  const iv = crypto.generateLegacyIV('aes-256-cbc', 'secret');

  const c = crypto.createCipheriv('aes-256-cbc', key, iv);
  const s = c.update('test', 'utf8', 'base64') + c.final('base64');
  assert.strictEqual(s, '375oxUQCIocvxmC5At+rvA==');
}

// Calling Cipher.final() or Decipher.final() twice should error but
// not assert. See #4886.
{
  const key = crypto.generateLegacyKey('aes-256-cbc', 'secret');
  const iv = crypto.generateLegacyIV('aes-256-cbc', 'secret');

  const c = crypto.createCipheriv('aes-256-cbc', key, iv);
  try { c.final('xxx'); } catch (e) { /* Ignore. */ }
  try { c.final('xxx'); } catch (e) { /* Ignore. */ }
  try { c.final('xxx'); } catch (e) { /* Ignore. */ }
  const d = crypto.createDecipheriv('aes-256-cbc', key, iv);
  try { d.final('xxx'); } catch (e) { /* Ignore. */ }
  try { d.final('xxx'); } catch (e) { /* Ignore. */ }
  try { d.final('xxx'); } catch (e) { /* Ignore. */ }
}

// Regression test for #5482: string to Cipher#update() should not assert.
{
  const key = crypto.generateLegacyKey('aes-192-cbc', '0123456789abcdef');
  const iv = crypto.generateLegacyIV('aes-192-cbc', '0123456789abcdef');

  const c = crypto.createCipheriv('aes-192-cbc', key, iv);
  c.update('update');
  c.final();
}

// #5655 regression tests, 'utf-8' and 'utf8' are identical.
{
  const key = crypto.generateLegacyKey('aes-192-cbc', '0123456789abcdef');
  const iv = crypto.generateLegacyIV('aes-192-cbc', '0123456789abcdef');

  let c = crypto.createCipheriv('aes-192-cbc', key, iv);
  c.update('update', '');  // Defaults to "utf8".
  c.final('utf-8');  // Should not throw.

  c = crypto.createCipheriv('aes-192-cbc', key, iv);
  c.update('update', 'utf8');
  c.final('utf-8');  // Should not throw.

  c = crypto.createCipheriv('aes-192-cbc', key, iv);
  c.update('update', 'utf-8');
  c.final('utf8');  // Should not throw.
}

// Regression tests for https://github.com/nodejs/node/issues/8236.
{
  const key = crypto.generateLegacyKey('aes-192-cbc', '0123456789abcdef');
  const iv = crypto.generateLegacyIV('aes-192-cbc', '0123456789abcdef');

  const plaintext = 'Top secret!!!';
  const c = crypto.createCipheriv('aes-192-cbc', key, iv);
  let ciph = c.update(plaintext, 'utf16le', 'base64');
  ciph += c.final('base64');

  let decipher = crypto.createDecipheriv('aes-192-cbc', key, iv);

  let txt;
  assert.doesNotThrow(() => txt = decipher.update(ciph, 'base64', 'ucs2'));
  assert.doesNotThrow(() => txt += decipher.final('ucs2'));
  assert.strictEqual(txt, plaintext, 'decrypted result in ucs2');

  decipher = crypto.createDecipheriv('aes-192-cbc', key, iv);
  assert.doesNotThrow(() => txt = decipher.update(ciph, 'base64', 'ucs-2'));
  assert.doesNotThrow(() => txt += decipher.final('ucs-2'));
  assert.strictEqual(txt, plaintext, 'decrypted result in ucs-2');

  decipher = crypto.createDecipheriv('aes-192-cbc', key, iv);
  assert.doesNotThrow(() => txt = decipher.update(ciph, 'base64', 'utf-16le'));
  assert.doesNotThrow(() => txt += decipher.final('utf-16le'));
  assert.strictEqual(txt, plaintext, 'decrypted result in utf-16le');
}

// setAutoPadding/setAuthTag/setAAD should return `this`
{
  const key = '0123456789';
  const tagbuf = Buffer.from('tagbuf');
  const aadbuf = Buffer.from('aadbuf');
  const decipher = crypto.createDecipher('aes-256-gcm', key);
  assert.strictEqual(decipher.setAutoPadding(), decipher);
  assert.strictEqual(decipher.setAuthTag(tagbuf), decipher);
  assert.strictEqual(decipher.setAAD(aadbuf), decipher);
}

// error throwing in setAAD/setAuthTag/getAuthTag/setAutoPadding
{
  const key = crypto.generateLegacyKey('aes-256-gcm', '0123456789');
  const iv = crypto.generateLegacyIV('aes-256-gcm', '0123456789');

  const aadbuf = Buffer.from('aadbuf');
  const data = Buffer.from('test-crypto-cipher-decipher');

  const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
  cipher.setAAD(aadbuf);
  cipher.setAutoPadding();

  assert.throws(() => {
    cipher.getAuthTag();
  }, /^Error: Attempting to get auth tag in unsupported state$/);

  const encrypted = Buffer.concat([cipher.update(data), cipher.final()]);

  const decipher = crypto.createDecipheriv('aes-256-gcm', key, iv);
  decipher.setAAD(aadbuf);
  decipher.setAuthTag(cipher.getAuthTag());
  decipher.setAutoPadding();
  decipher.update(encrypted);
  decipher.final();

  assert.throws(() => {
    decipher.setAAD(aadbuf);
  }, /^Error: Attempting to set AAD in unsupported state$/);

  assert.throws(() => {
    decipher.setAuthTag(cipher.getAuthTag());
  }, /^Error: Attempting to set auth tag in unsupported state$/);

  assert.throws(() => {
    decipher.setAutoPadding();
  }, /^Error: Attempting to set auto padding in unsupported state$/);
}
