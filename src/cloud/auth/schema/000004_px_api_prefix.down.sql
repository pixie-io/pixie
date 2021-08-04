UPDATE api_keys
   SET unsalted_key = REPLACE(unsalted_key, 'px-api-')
 WHERE unsalted_key LIKE 'px-api-%';
