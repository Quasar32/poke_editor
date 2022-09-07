def Settings( **kwargs ):
  return {
    'flags': [ 
        '-x', 
        'c', 
        '-I',
        'lib/glfw/include',
        '-I',
        'lib/glad/include'
    ],
  }
