const fs = require('fs');
const { exec } = require('child_process');
const path = require('path');

// Get the file path from command line arguments
const inputFile = process.argv[2];

if (!inputFile) {
    console.error('Please provide an input file path as an argument');
    process.exit(1);
}

// Create a temporary output file name
const tmpOutputFile = 'output.js';

// Construct the bundle command
const bundleCommand = `yarn react-native bundle \
--entry-file ${inputFile} \
--bundle-output ${tmpOutputFile} \
--dev true \
--platform android \
--minify false`;

// Execute the bundle command
exec(bundleCommand, (error, stdout, stderr) => {
    if (error) {
        console.error('Error during bundling:', error.message);
        process.exit(1);
    }
    
    if (stderr) {
        console.error('Bundler stderr:', stderr);
    }

    try {
        // Read the generated bundle
        const content = fs.readFileSync(tmpOutputFile, 'utf8');
        
        // Output the content in the desired format
        console.log(`// eslint-disable-next-line
export default ${JSON.stringify(content)};`);
        
        // Clean up the temporary file
        fs.unlinkSync(tmpOutputFile);
    } catch (readError) {
        console.error('Error processing bundle:', readError.message);
        // Try to clean up even if there was an error
        try {
            if (fs.existsSync(tmpOutputFile)) {
                fs.unlinkSync(tmpOutputFile);
            }
        } catch (cleanupError) {
            console.error('Error during cleanup:', cleanupError.message);
        }
        process.exit(1);
    }
});