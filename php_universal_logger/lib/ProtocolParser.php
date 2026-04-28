<?php

class ProtocolParser {
    private $config;
    private $headerSize;
    private $magic;

    public function __construct($configPath) {
        $json = file_get_contents($configPath);
        $this->config = json_decode($json, true);
        $this->headerSize = $this->config['PROTO']['HEADER_SIZE'];
        $this->magic = $this->config['PROTO']['MAGIC'];
    }

    /**
     * Decodes a packet (binary or hex string)
     */
    public function decode($data) {
        // If data is a hex string, convert to binary
        if (is_string($data) && preg_match('/^[0-9a-fA-F]+$/', $data)) {
            $data = hex2bin($data);
        }

        // Check if data is JSON
        if ($this->isJson($data)) {
            return json_decode($data, true);
        }

        // Binary decoding
        return $this->decodeBinary($data);
    }

    private function isJson($string) {
        if (!is_string($string)) return false;
        json_decode($string);
        return (json_last_error() == JSON_ERROR_NONE);
    }

    private function decodeBinary($buffer) {
        if (strlen($buffer) < $this->headerSize) {
            throw new Exception("Buffer too short");
        }

        // Parse Header
        // 0: Magic (uint8)
        // 1: Version (uint8)
        // 2: Type (uint8)
        // 3-4: Payload Length (uint16le)
        $header = unpack('Cmagic/Cversion/Ctype/vlen', $buffer);

        if ($header['magic'] !== $this->magic) {
            throw new Exception("Invalid Magic Byte: " . $header['magic']);
        }

        $type = $header['type'];
        $payload = substr($buffer, $this->headerSize, $header['len']);
        
        $typeKey = "TYPE_" . $type;
        if (!isset($this->config['PARSER_SPECS'][$typeKey])) {
            return [
                'meta' => $header,
                'data' => null,
                'error' => "Unknown type: " . $type
            ];
        }

        $spec = $this->config['PARSER_SPECS'][$typeKey];
        $fields = $spec['fields'];
        $decoded = [];

        foreach ($fields as $field) {
            $val = $this->readField($payload, $field);
            
            // Scaling
            if (isset($field['div'])) $val = $val / $field['div'];
            if (isset($field['sub'])) $val = $val - $field['sub'];
            
            // Rounding
            if (is_float($val)) $val = round($val, 2);

            $decoded[$field['name']] = $val;
        }

        return [
            'type' => $spec['name'],
            'typeId' => $type,
            'version' => $header['version'],
            'data' => $decoded
        ];
    }

    private function readField($buffer, $field) {
        $offset = $field['offset'];
        $type = $field['type'];

        switch ($type) {
            case 'uint8':
                return unpack('C', substr($buffer, $offset, 1))[1];
            case 'uint16le':
                return unpack('v', substr($buffer, $offset, 2))[1];
            case 'int16le':
                $val = unpack('v', substr($buffer, $offset, 2))[1];
                return ($val >= 32768) ? $val - 65536 : $val;
            case 'uint32le':
                return unpack('V', substr($buffer, $offset, 4))[1];
            case 'floatle':
                return unpack('g', substr($buffer, $offset, 4))[1];
            default:
                throw new Exception("Unsupported type: " . $type);
        }
    }
}
