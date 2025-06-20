import React, { useState, useMemo } from 'react';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { useVirtualizer } from '@tanstack/react-virtual';
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from './components/ui/card';
import { Input } from './components/ui/input';
import { Button } from './components/ui/button';
import { Badge } from './components/ui/badge';
import { Tabs, TabsContent, TabsList, TabsTrigger } from './components/ui/tabs';
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from './components/ui/select';
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from './components/ui/dropdown-menu';
import {
  Search,
  Filter,
  Plus,
  MoreVertical,
  FileJson,
  Clock,
  User,
  Library,
  Hash,
} from 'lucide-react';
import { apiClient } from './lib/api';
import { formatDistanceToNow } from 'date-fns';
import { JsonViewer } from './components/JsonViewer';

interface Document {
  uuid: string;
  type: string;
  library: string;
  collection: string;
  owner: string;
  created_at: string;
  modified_at: string;
  name?: string;
  data: any;
}

interface Library {
  uuid: string;
  name: string;
  settings: any;
}

export function DocumentBrowser() {
  const [selectedLibrary, setSelectedLibrary] = useState('default');
  const [selectedType, setSelectedType] = useState('all');
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedDocument, setSelectedDocument] = useState<Document | null>(null);
  const queryClient = useQueryClient();

  // Fetch libraries
  const { data: libraries = [] } = useQuery({
    queryKey: ['libraries'],
    queryFn: () => apiClient.get<Library[]>('/api/libraries'),
  });

  // Fetch document types (collections)
  const { data: documentTypes = [] } = useQuery({
    queryKey: ['document-types', selectedLibrary],
    queryFn: () => apiClient.get<string[]>(`/api/collections?library=${selectedLibrary}`),
  });

  // Fetch documents with filtering
  const { data: documentsResponse, isLoading } = useQuery({
    queryKey: ['documents', selectedLibrary, selectedType, searchQuery],
    queryFn: () => {
      const params = new URLSearchParams({
        library: selectedLibrary,
        ...(selectedType !== 'all' && { type: selectedType }),
        ...(searchQuery && { search: searchQuery }),
        limit: '1000',
      });
      return apiClient.get<{ documents: Document[]; count: number }>(
        `/api/documents?${params}`
      );
    },
  });

  const documents = documentsResponse?.documents || [];

  // Virtual scrolling for performance
  const parentRef = React.useRef<HTMLDivElement>(null);
  const virtualizer = useVirtualizer({
    count: documents.length,
    getScrollElement: () => parentRef.current,
    estimateSize: () => 80,
    overscan: 5,
  });

  // Delete document mutation
  const deleteMutation = useMutation({
    mutationFn: (uuid: string) => apiClient.delete(`/api/documents/${uuid}`),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['documents'] });
      setSelectedDocument(null);
    },
  });

  const getDocumentIcon = (type: string) => {
    switch (type) {
      case 'user':
        return <User className="h-4 w-4" />;
      case 'library':
        return <Library className="h-4 w-4" />;
      default:
        return <FileJson className="h-4 w-4" />;
    }
  };

  const getDocumentName = (doc: Document) => {
    return doc.name || doc.data?.name || doc.data?.title || doc.uuid;
  };

  return (
    <div className="flex h-[calc(100vh-4rem)] gap-4 p-4">
      {/* Document List */}
      <div className="w-1/3 min-w-[300px] flex flex-col">
        <Card className="flex-1 flex flex-col">
          <CardHeader>
            <CardTitle>Documents</CardTitle>
            <CardDescription>
              Browse and manage documents in the unified collection
            </CardDescription>
          </CardHeader>
          <CardContent className="flex-1 flex flex-col space-y-4">
            {/* Filters */}
            <div className="space-y-2">
              <div className="flex gap-2">
                <Select value={selectedLibrary} onValueChange={setSelectedLibrary}>
                  <SelectTrigger className="w-[180px]">
                    <SelectValue placeholder="Select library" />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="default">Default</SelectItem>
                    <SelectItem value="system">System</SelectItem>
                    {libraries.map((lib) => (
                      <SelectItem key={lib.uuid} value={lib.name}>
                        {lib.name}
                      </SelectItem>
                    ))}
                  </SelectContent>
                </Select>

                <Select value={selectedType} onValueChange={setSelectedType}>
                  <SelectTrigger className="w-[180px]">
                    <SelectValue placeholder="Document type" />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="all">All Types</SelectItem>
                    {documentTypes.map((type) => (
                      <SelectItem key={type} value={type}>
                        {type}
                      </SelectItem>
                    ))}
                  </SelectContent>
                </Select>
              </div>

              <div className="relative">
                <Search className="absolute left-2 top-2.5 h-4 w-4 text-muted-foreground" />
                <Input
                  placeholder="Search documents..."
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  className="pl-8"
                />
              </div>
            </div>

            {/* Document List with Virtual Scrolling */}
            <div
              ref={parentRef}
              className="flex-1 overflow-auto border rounded-md"
            >
              <div
                style={{
                  height: `${virtualizer.getTotalSize()}px`,
                  width: '100%',
                  position: 'relative',
                }}
              >
                {virtualizer.getVirtualItems().map((virtualItem) => {
                  const doc = documents[virtualItem.index];
                  return (
                    <div
                      key={virtualItem.key}
                      style={{
                        position: 'absolute',
                        top: 0,
                        left: 0,
                        width: '100%',
                        height: `${virtualItem.size}px`,
                        transform: `translateY(${virtualItem.start}px)`,
                      }}
                    >
                      <div
                        className={`p-3 border-b cursor-pointer hover:bg-accent/5 transition-colors ${
                          selectedDocument?.uuid === doc.uuid ? 'bg-accent/10' : ''
                        }`}
                        onClick={() => setSelectedDocument(doc)}
                      >
                        <div className="flex items-start justify-between">
                          <div className="flex-1 min-w-0">
                            <div className="flex items-center gap-2">
                              {getDocumentIcon(doc.type)}
                              <span className="font-medium truncate">
                                {getDocumentName(doc)}
                              </span>
                            </div>
                            <div className="flex items-center gap-4 mt-1 text-xs text-muted-foreground">
                              <span className="flex items-center gap-1">
                                <Hash className="h-3 w-3" />
                                {doc.type}
                              </span>
                              <span className="flex items-center gap-1">
                                <Clock className="h-3 w-3" />
                                {formatDistanceToNow(new Date(doc.modified_at), {
                                  addSuffix: true,
                                })}
                              </span>
                            </div>
                          </div>
                          <Badge variant="outline" className="ml-2">
                            {doc.library}
                          </Badge>
                        </div>
                      </div>
                    </div>
                  );
                })}
              </div>
            </div>

            {/* Results count */}
            <div className="text-sm text-muted-foreground">
              {documents.length} documents found
            </div>
          </CardContent>
        </Card>
      </div>

      {/* Document Details */}
      <div className="flex-1">
        {selectedDocument ? (
          <Card className="h-full">
            <CardHeader>
              <div className="flex items-center justify-between">
                <div>
                  <CardTitle className="flex items-center gap-2">
                    {getDocumentIcon(selectedDocument.type)}
                    {getDocumentName(selectedDocument)}
                  </CardTitle>
                  <CardDescription className="mt-1">
                    UUID: {selectedDocument.uuid}
                  </CardDescription>
                </div>
                <DropdownMenu>
                  <DropdownMenuTrigger asChild>
                    <Button variant="ghost" size="icon">
                      <MoreVertical className="h-4 w-4" />
                    </Button>
                  </DropdownMenuTrigger>
                  <DropdownMenuContent align="end">
                    <DropdownMenuItem>Edit</DropdownMenuItem>
                    <DropdownMenuItem>Duplicate</DropdownMenuItem>
                    <DropdownMenuItem
                      className="text-destructive"
                      onClick={() => deleteMutation.mutate(selectedDocument.uuid)}
                    >
                      Delete
                    </DropdownMenuItem>
                  </DropdownMenuContent>
                </DropdownMenu>
              </div>
            </CardHeader>
            <CardContent>
              <Tabs defaultValue="details" className="h-full">
                <TabsList>
                  <TabsTrigger value="details">Details</TabsTrigger>
                  <TabsTrigger value="json">JSON</TabsTrigger>
                  <TabsTrigger value="metadata">Metadata</TabsTrigger>
                </TabsList>

                <TabsContent value="details" className="space-y-4">
                  <div className="grid grid-cols-2 gap-4">
                    <div>
                      <label className="text-sm font-medium">Type</label>
                      <p className="text-sm text-muted-foreground">
                        {selectedDocument.type}
                      </p>
                    </div>
                    <div>
                      <label className="text-sm font-medium">Library</label>
                      <p className="text-sm text-muted-foreground">
                        {selectedDocument.library}
                      </p>
                    </div>
                    <div>
                      <label className="text-sm font-medium">Owner</label>
                      <p className="text-sm text-muted-foreground">
                        {selectedDocument.owner}
                      </p>
                    </div>
                    <div>
                      <label className="text-sm font-medium">Collection</label>
                      <p className="text-sm text-muted-foreground">
                        {selectedDocument.collection}
                      </p>
                    </div>
                    <div>
                      <label className="text-sm font-medium">Created</label>
                      <p className="text-sm text-muted-foreground">
                        {new Date(selectedDocument.created_at).toLocaleString()}
                      </p>
                    </div>
                    <div>
                      <label className="text-sm font-medium">Modified</label>
                      <p className="text-sm text-muted-foreground">
                        {new Date(selectedDocument.modified_at).toLocaleString()}
                      </p>
                    </div>
                  </div>
                </TabsContent>

                <TabsContent value="json" className="h-[calc(100%-3rem)]">
                  <JsonViewer data={selectedDocument.data} />
                </TabsContent>

                <TabsContent value="metadata" className="h-[calc(100%-3rem)]">
                  <JsonViewer
                    data={{
                      uuid: selectedDocument.uuid,
                      type: selectedDocument.type,
                      library: selectedDocument.library,
                      collection: selectedDocument.collection,
                      owner: selectedDocument.owner,
                      created_at: selectedDocument.created_at,
                      modified_at: selectedDocument.modified_at,
                    }}
                  />
                </TabsContent>
              </Tabs>
            </CardContent>
          </Card>
        ) : (
          <Card className="h-full flex items-center justify-center">
            <CardContent>
              <p className="text-muted-foreground">
                Select a document to view details
              </p>
            </CardContent>
          </Card>
        )}
      </div>
    </div>
  );
}