# revise_database Tool

C++ utility to reset the `summarized` field to `false` for all documents in the Elasticsearch `perception_context` index.

## Features

- ? Connect to Elasticsearch server
- ? Count document status statistics
- ? Batch update all documents
- ? Dry-run mode
- ? Detailed error handling and logging
- ? Confirmation before updating

## Build

Already included in the CMake build:

```bash
cd windows_code/buildnew
cmake ..
cmake --build . --target revise_database --config Release
```

The generated executable is located at:
```
buildnew/bin/Release/revise_database.exe
```

## Usage

### Basic Usage

```bash
cd buildnew/bin/Release
./revise_database.exe
```

This will:
1. Connect to local Elasticsearch (`localhost:9200`)
2. Display current status statistics
3. Ask for confirmation
4. Update all documents with `summarized=true` or missing the field to `summarized=false`
5. Display new status

### Command Line Arguments

| Argument | Description | Default |
|----------|-------------|---------|
| `--host HOST` | Elasticsearch host address | `localhost` |
| `--port PORT` | Elasticsearch port | `9200` |
| `--index INDEX` | Index name | `perception_context` |
| `--batch-size N` | Batch size for updates | `100` |
| `--dry-run` | Dry run mode (no actual changes) | `false` |
| `--help, -h` | Show help message | - |

### Usage Examples

#### 1. Dry Run (Recommended First)

View what would be affected without making changes:

```bash
./revise_database.exe --dry-run
```

Example output:
```
========================================
 revise_database - Reset Summarized Flag
========================================

Configuration:
  Host: localhost
  Port: 9200
  Index: perception_context
  Batch size: 100
  Dry run: Yes

Connecting to Elasticsearch at http://localhost:9200...
? Connected to Elasticsearch

Current status:
  Total documents: 1500
  Summarized=true: 250
  Summarized=false: 1200
  Missing field: 50

Documents to update: 300

[DRY RUN] Would update 300 documents
```

#### 2. Actual Execution

```bash
./revise_database.exe
```

Will update all documents after confirmation.

#### 3. Remote Elasticsearch

Connect to a remote server:

```bash
./revise_database.exe --host 192.168.1.100 --port 9200
```

#### 4. Custom Index

Specify a different index name:

```bash
./revise_database.exe --index my_custom_index
```

#### 5. Change Batch Size

Adjust batch size when updating many documents:

```bash
./revise_database.exe --batch-size 500
```

## Example Output

### Successful Execution

```
========================================
 revise_database - Reset Summarized Flag
========================================

Configuration:
  Host: localhost
  Port: 9200
  Index: perception_context
  Batch size: 100
  Dry run: No

Connecting to Elasticsearch at http://localhost:9200...
? Connected to Elasticsearch

Current status:
  Total documents: 1500
  Summarized=true: 250
  Summarized=false: 1200
  Missing field: 50

Documents to update: 300

This will update all documents in the index.
Continue? (yes/no): yes

Updating documents...
Processing batch of 300 documents...
? Successfully updated 300 documents

New status:
  Total documents: 1500
  Summarized=true: 0
  Summarized=false: 1500
  Missing field: 0

? Operation completed
```

### All Already False

```
Current status:
  Total documents: 1500
  Summarized=true: 0
  Summarized=false: 1500
  Missing field: 0

? All documents already have summarized=false
```

### No Documents

```
Current status:
  Total documents: 0
  Summarized=true: 0
  Summarized=false: 0
  Missing field: 0

? No documents found in index
```

## How It Works

The tool uses the `database_client` library to interact with Elasticsearch:

1. **Connection Phase**: Uses `DatabaseClientFactory` to create ES client
2. **Statistics Phase**: Executes multiple queries to count documents with different statuses
3. **Update Phase**: Batch retrieves and updates documents that need updating
4. **Verification Phase**: Re-counts statistics to confirm successful update

### Query Logic

Find documents that need updating:
```json
{
  "query": {
    "bool": {
      "should": [
        {"term": {"summarized": true}},
        {"bool": {"must_not": {"exists": {"field": "summarized"}}}}
      ]
    }
  }
}
```

### Update Operation

Uses Elasticsearch Update API:
```json
{
  "doc": {
    "summarized": false
  }
}
```

## Integration with LinguaCore

### Complete Workflow

1. **Stop LinguaCore service**
   ```bash
   # Stop LinguaCore.exe in Windows Task Manager
   # Or use Ctrl+C to stop running instance
   ```

2. **Run revise_database**
   ```bash
   cd buildnew/bin/Release
   ./revise_database.exe
   ```

3. **Start LinguaCore service**
   ```bash
   ./LinguaCore.exe
   ```

4. **Monitor logs**
   - Observe LinguaCore logs
   - Confirm it starts processing events
   - Check data growth in Qdrant

### Verification

View in Qdrant Web UI:
```
http://localhost:6333/dashboard
```

Check if the points count in collection `perception_summaries` is growing.

## Dependencies

- **database_client**: Elasticsearch client library
- **nlohmann_json**: JSON parsing
- **libcurl**: HTTP communication (via database_client)

These dependencies are automatically handled by CMake.

## Troubleshooting

### Connection Failed

```
? Error: Failed to connect to Elasticsearch
```

**Solutions**:
- Ensure Elasticsearch is running
- Verify host address and port
- Check firewall settings

### Index Not Found

```
? Error: Index not found
```

**Solutions**:
- Verify index name is correct
- Use `--index` parameter to specify correct index

### Update Failed

```
Failed to update document: event_123
```

**Solutions**:
- Check if document exists
- Verify permissions
- Check Elasticsearch logs

## Safety Guidelines

1. **Always use --dry-run**: View what would change before making actual changes
2. **Stop LinguaCore**: Avoid conflicts while modifying data
3. **Backup data**: Backup Elasticsearch for important data
4. **Verify results**: Observe and confirm update succeeded

## Performance Optimization

For large datasets (>10,000 documents):

1. **Increase batch size**:
   ```bash
   ./revise_database.exe --batch-size 1000
   ```

2. **Schedule off-peak**: Execute during low load times

3. **Monitor ES load**: Ensure Elasticsearch has sufficient resources

## Build Troubleshooting

### Issue 1: Cannot Build

```
CMake Error: Cannot find source file: ReviseDatabase.cpp
```

**Solution**:
```bash
# Ensure file is in correct location
ls tools/revise_database/ReviseDatabase.cpp

# Reconfigure CMake
cd buildnew
rm -rf *
cmake ..
```

### Issue 2: Missing DLL at Runtime

```
The code execution cannot proceed because database_client.dll was not found
```

**Solution**:
```bash
# Ensure running from correct directory
cd buildnew/bin/Release

# Or add DLL directory to PATH
```

### Issue 3: Permission Denied

```
? Error: Unauthorized access
```

**Solutions**:
- Check Elasticsearch authentication settings
- Ensure write permissions
- Verify index permissions

## Development and Debugging

### Key Functions

Main functions:
- `countDocuments()` - Count document statuses
- `resetAllToFalse()` - Execute batch update
- `parseArguments()` - Parse command line arguments

### Debug Mode

Debug with Visual Studio:
```bash
# Open PerceptionEngine.sln in Visual Studio
# Set revise_database as startup project
# Set breakpoints and press F5 to debug
```

### Add Logging

Add more detailed logs in code:
```cpp
std::cout << "Debug: Processing document " << event.eventId << std::endl;
```

## Comparison with Python Script

| Feature | C++ Tool | Python Script |
|---------|----------|---------------|
| Speed | Fast | Moderate |
| Deployment | Pre-compiled | Requires pip packages |
| Execution | Standalone exe | Requires Python runtime |
| Integration | Same codebase as LinguaCore | Separate script |
| Maintenance | C++ knowledge | Python knowledge |

Both tools provide the same functionality, choose based on preference.

## License

Same license as the Perception Engine project.

## Version History

- **v1.0** (2024) - Initial version
  - Basic reset functionality
  - Dry run mode
  - Batch update support

---

**Author**: Perception Engine Team  
**Date**: 2024
