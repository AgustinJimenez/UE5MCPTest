// Script to trigger reconstruction on all LevelBlock instances
const net = require('net');

const instances = [
  'LevelBlock_C_1', 'LevelBlock_C_0', 'LevelBlock_C_4', 'LevelBlock_C_5',
  'LevelBlock_C_6', 'LevelBlock_C_11', 'LevelBlock_C_2', 'LevelBlock_C_8',
  'LevelBlock_C_10', 'LevelBlock_C_12', 'LevelBlock_C_13', 'LevelBlock_C_14',
  'LevelBlock_C_27', 'LevelBlock_C_29', 'LevelBlock_C_3', 'LevelBlock_C_9',
  'LevelBlock_C_15', 'LevelBlock_C_23', 'LevelBlock_C_61', 'LevelBlock_C_62',
  'LevelBlock_C_63', 'LevelBlock_C_64', 'LevelBlock_C_65', 'LevelBlock_C_66',
  'LevelBlock_C_67', 'LevelBlock_C_68', 'LevelBlock_C_69', 'LevelBlock_C_70',
  'LevelBlock_C_71', 'LevelBlock_C_72', 'LevelBlock_C_73', 'LevelBlock_C_74',
  'LevelBlock_C_7'
];

async function triggerReconstruction(actorName) {
  return new Promise((resolve, reject) => {
    const client = new net.Socket();
    client.connect(9877, '127.0.0.1', () => {
      const command = {
        command: 'set_actor_properties',
        params: {
          actor_name: actorName,
          properties: { 'bHidden': 'False' }
        }
      };
      client.write(JSON.stringify(command) + '\n');
    });

    let data = '';
    client.on('data', (chunk) => { data += chunk.toString(); });
    client.on('close', () => {
      try {
        const response = JSON.parse(data);
        resolve(response);
      } catch (e) {
        reject(e);
      }
    });
    client.on('error', reject);
  });
}

async function reconstructAll() {
  console.log(`Triggering reconstruction for ${instances.length} instances...`);

  for (const instance of instances) {
    try {
      await triggerReconstruction(instance);
      console.log(`✓ ${instance}`);
    } catch (error) {
      console.error(`✗ ${instance}:`, error.message);
    }
  }

  console.log(`\n✓ All instances reconstructed`);
}

reconstructAll().catch(console.error);
