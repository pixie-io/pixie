PREPARE pstmt FROM 'select table_schema as database_name, table_name
from information_schema.tables
where table_type = ? and table_schema = ?
order by database_name, table_name';

SET @tableType = 'BASE TABLE';
SET @tableSchema = 'mysql';
EXECUTE pstmt USING @tableType, @tableSchema;

-- Another invocation with different inputs
SET @tableSchema = 'bogus';
EXECUTE pstmt USING @tableType, @tableSchema;

DEALLOCATE PREPARE pstmt;
