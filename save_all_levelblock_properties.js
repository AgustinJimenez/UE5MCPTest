// Script to save all LevelBlock instance properties
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

const savedProperties = {};

async function readActorProperties(actorName) {
  return new Promise((resolve, reject) => {
    const client = new net.Socket();
    client.connect(9877, '127.0.0.1', () => {
      const command = {
        command: 'read_actor_properties',
        params: { actor_name: actorName }
      };
      client.write(JSON.stringify(command) + '\n');
    });

    let data = '';
    client.on('data', (chunk) => {
      data += chunk.toString();
    });

    client.on('close', () => {
      try {
        const response = JSON.parse(data);
        if (response.success) {
          resolve(response.data.properties);
        } else {
          reject(new Error(response.error));
        }
      } catch (e) {
        reject(e);
      }
    });

    client.on('error', reject);
  });
}

async function saveAllProperties() {
  console.log(`Saving properties for ${instances.length} LevelBlock instances...`);

  for (const instance of instances) {
    try {
      const properties = await readActorProperties(instance);
      savedProperties[instance] = properties;
      console.log(`✓ Saved ${instance}`);
    } catch (error) {
      console.error(`✗ Failed ${instance}:`, error.message);
    }
  }

  // Save to JSON file
  const fs = require('fs');
  fs.writeFileSync('levelblock_properties_backup.json', JSON.stringify(savedProperties, null, 2));
  console.log(`\n✓ All properties saved to levelblock_properties_backup.json`);
}

saveAllProperties().catch(console.error);
