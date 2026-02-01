// Script to restore all LevelBlock instance properties
const net = require('net');
const fs = require('fs');

// Load saved properties
const savedProperties = JSON.parse(fs.readFileSync('levelblock_properties_backup.json', 'utf8'));

async function setActorProperties(actorName, properties) {
  return new Promise((resolve, reject) => {
    const client = new net.Socket();
    client.connect(9877, '127.0.0.1', () => {
      const command = {
        command: 'set_actor_properties',
        params: {
          actor_name: actorName,
          properties: properties
        }
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
          resolve(response.data);
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

async function restoreAllProperties() {
  const instances = Object.keys(savedProperties);
  console.log(`Restoring properties for ${instances.length} LevelBlock instances...`);

  let successCount = 0;
  let failCount = 0;

  for (const instance of instances) {
    try {
      const result = await setActorProperties(instance, savedProperties[instance]);
      console.log(`✓ Restored ${instance} (${result.properties_set} properties)`);
      successCount++;
    } catch (error) {
      console.error(`✗ Failed ${instance}:`, error.message);
      failCount++;
    }
  }

  console.log(`\n✓ Success: ${successCount} instances`);
  if (failCount > 0) {
    console.log(`✗ Failed: ${failCount} instances`);
  }
}

restoreAllProperties().catch(console.error);
