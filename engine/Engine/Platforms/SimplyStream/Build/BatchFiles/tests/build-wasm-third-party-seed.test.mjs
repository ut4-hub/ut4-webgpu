import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { chmod, copyFile, mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import test from 'node:test';

const buildScript = new URL('../Build_SimplyStream_Wasm_ThirdParty.sh', import.meta.url);

async function write(path, contents, executable = false) {
	await mkdir(dirname(path), { recursive: true });
	await writeFile(path, contents);
	if (executable) await chmod(path, 0o755);
}

async function makeFixture() {
	const root = await mkdtemp(join(tmpdir(), 'simplystream-third-party-seed-'));
	const platform = join(root, 'Engine/Platforms/SimplyStream');
	const script = join(platform, 'Build/BatchFiles/Build_SimplyStream_Wasm_ThirdParty.sh');
	await mkdir(dirname(script), { recursive: true });
	await copyFile(buildScript, script);
	await chmod(script, 0o755);

	const archivePath = 'Source/ThirdParty/Blake3/Test/wasm32/libBLAKE3.a';
	const archive = Buffer.from('seeded wasm archive');
	const sha256 = createHash('sha256').update(archive).digest('hex');
	const seed = join(root, 'seed');
	await write(join(seed, archivePath), archive);
	await write(join(platform, 'Build/BatchFiles/StrataSourcePrepare.json'), JSON.stringify({
		version: 1,
		thirdParty: {
			memoryMode: 0,
			libraries: ['blake3'],
			archives: [{ library: 'blake3', path: archivePath, sha256 }],
		},
	}));

	const tools = join(root, 'emscripten');
	const successTool = '#!/usr/bin/env bash\nexit 0\n';
	for (const name of ['emcc', 'em++', 'emcmake', 'emar', 'ninja', 'unzip']) {
		await write(join(tools, name), successTool, true);
	}
	await write(join(tools, 'cmake'), '#!/usr/bin/env bash\nexit 97\n', true);
	await write(join(root, 'bin/llvm-readobj'), '#!/usr/bin/env bash\nprintf "Arch: wasm32\\n"\n', true);
	await write(join(root, 'bin/llvm-nm'), successTool, true);
	return { root, platform, script, seed, archivePath, tools };
}

function run(fixture) {
	return spawnSync('bash', [fixture.script, '--memory-mode', '0', '--only', 'blake3', '--seed-platform-root', fixture.seed], {
		encoding: 'utf8',
		env: { ...process.env, PATH: `${fixture.tools}:${process.env.PATH}` },
	});
}

test('seed reuse requires the exact locked hash and still verifies the wasm ABI', async (t) => {
	const fixture = await makeFixture();
	t.after(() => rm(fixture.root, { recursive: true, force: true }));
	const result = run(fixture);
	assert.equal(result.status, 0, result.stderr || result.stdout);
	assert.match(result.stdout, /reused hash-locked blake3 archive/);
	assert.equal(await readFile(join(fixture.platform, fixture.archivePath), 'utf8'), 'seeded wasm archive');
});

test('a seed hash mismatch falls back to the source recipe', async (t) => {
	const fixture = await makeFixture();
	t.after(() => rm(fixture.root, { recursive: true, force: true }));
	await writeFile(join(fixture.seed, fixture.archivePath), 'changed seed');
	const result = run(fixture);
	assert.equal(result.status, 97);
	assert.match(result.stdout, /building blake3/);
});
